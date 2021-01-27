#include "engine.hpp"
#include "move_picker.hpp"

void TimeControl::initialize(const GoParameters& go, Color own, int ply) {
  start = now();
  double duration = kInfDuration;
  if (go.movetime != 0) {
    duration = std::min(duration, (double)go.movetime);
  }
  if (go.time[own] != 0) {
    double time = go.time[own];
    double inc = go.inc[own];
    int cnt = go.movestogo ? go.movestogo : std::max(10, 32 - ply / 2);
    duration = std::min(duration, (time + inc * (cnt - 1)) / cnt); // Split remaining time to each move

    if (ply <= 8) {
      double opening_time = 1000.0 + (1000. / 8.) * ply;
      duration = std::min(duration, opening_time);
    }
  }
  finish = start + Msec(int64_t(kSafeFactor * duration));
};

void SearchResult::print(std::ostream& ostr) const {
  if (type == kSearchResultInfo) {
    if (!debug.empty()) {
      ostr << toString("info string DEBUG", debug);

    } else {
      ostr << toString(
        "info depth", depth,
        "score cp", score,
        "time", stats_time,
        "nodes", stats_nodes,
        "nps", (1000 * stats_nodes) / stats_time,
        "pv"
      );
      for (auto move : pv) { ostr << " " << move; }
    }
  }

  if (type == kSearchResultBestMove) {
    ASSERT(pv.size() > 0);
    ostr << toString("bestmove", pv.data[0]);
  }
}

void Engine::print(std::ostream& ostr) {
  ostr << ":: Position" << "\n";
  ostr << position;
  ostr << ":: Evaluation" << "\n";
  ostr << evaluator.evaluate() << "\n";
  if (position.move_evaluator) {
    ostr << ":: Move" << "\n";
    MoveList moves;
    NNMoveScoreList nn_moves;
    position.generateMoves(moves);
    position.assignNNMoveScore(moves, nn_moves);
    for (auto [move, score] : nn_moves) { ostr << move << ": " << score << "\n"; }
  }
}

void Engine::stop() {
  if (!isRunning()) { return; }
  ASSERT(!stop_requested.load(std::memory_order_acquire));
  stop_requested.store(true, std::memory_order_release);
  wait();
  stop_requested.store(false, std::memory_order_release);
  ASSERT(!isRunning());
}

void Engine::wait() {
  ASSERT(go_thread_future.valid()); // Check Engine::go didn't meet with Engine::wait yet
  go_thread_future.wait();
  ASSERT(go_thread_future.get()); // "get" will invalidate "future"
  ASSERT(!go_thread_future.valid());
}

bool Engine::checkSearchLimit() {
  if (stop_requested.load(std::memory_order_acquire)) { return 0; }
  if (!time_control.checkLimit()) { return 0; }
  return 1;
}

void Engine::go(bool blocking) {
  ASSERT(!go_thread_future.valid()); // Check previous Engine::go met with Engine::wait
  go_thread_future = std::async([this]() { goImpl(); return true; });
  ASSERT(go_thread_future.valid());
  if (blocking) { wait(); }
}

void Engine::goImpl() {
  time_control.initialize(go_parameters, position.side_to_move, position.game_ply);

  if (debug) {
    SearchResult info;
    info.type = kSearchResultInfo;
    info.debug = toString(
      "ply", position.game_ply,
      "side", position.side_to_move,
      "eval", position.evaluate(),
      "time_limit", time_control.getDuration()
    );
    search_result_callback(info);
  }

  int depth_end = go_parameters.depth;
  ASSERT(depth_end > 0);
  results.assign(depth_end + 1, {});

  int best_index = 0;

  // Get random first move just in case we don't even have time for "depth 1"
  Move first_move = position.getRandomMove();
  ASSERT(first_move != kNoneMove);
  results[0] = { .type = kSearchResultInfo, .depth = 1, .score = 0, .stats_time = 1, .stats_nodes = 1 };
  results[0].pv.put(first_move);
  search_result_callback(results[0]);

  // Iterative deepening
  for (int depth = 1; depth <= depth_end; depth++) {
    SearchResult res;
    if (depth < 4) {
      res = search(depth);
    } else {
      res = searchWithAspirationWindow(depth, results[depth - 1].score);
    }
    if (!checkSearchLimit()) { break; } // Ignore possibly incomplete result

    // Save result and send "info ..."
    best_index = depth;
    results[depth] = res;
    results[depth].type = kSearchResultInfo;
    search_result_callback(results[depth]);

    // Debug info
    if (debug) {
      SearchResult res_info;
      res_info.type = kSearchResultInfo;
      res_info.debug = toString(
        "max_depth", res.stats_max_depth,
        "aspiration", res.stats_aspiration,
        "tt_hit", res.stats_tt_hit,
        "tt_cut", res.stats_tt_cut,
        "refutation", res.stats_refutation,
        "futility_prune", res.stats_futility_prune,
        "lmr", toString(res.stats_lmr_success) +  "/" + toString(res.stats_lmr)
      );
      search_result_callback(res_info);
    }
  }

  // Send "bestmove ..."
  SearchResult best = results[best_index];
  best.type = kSearchResultBestMove;
  search_result_callback(best);
}

SearchResult Engine::searchWithAspirationWindow(int depth, Score init_target) {
  const Score kInitDelta = 25;

  // Prevent overflow
  int32_t delta = kInitDelta;
  int32_t target = init_target;
  int32_t kInf = kScoreInf;
  int32_t alpha, beta;

  for (int i = 0; ; i++) {
    alpha = std::max(target - delta, -kInf);
    beta = std::min(target + delta, kInf);

    SearchResult res;
    res.depth = depth;

    state->reset();
    Score score = searchImpl(alpha, beta, 0, depth, res);
    if (!checkSearchLimit()) { return {}; }

    if (alpha < score && score < beta) {
      res.score = score;
      res.pv = state->pv;
      res.stats_time = time_control.getTime() + 1;
      res.stats_aspiration = i;
      return res;
    }

    // Extend low
    //       <--t-->
    // <-----t----->
    if (score <= alpha) { target -= delta; }

    // Extend high
    // <--t-->
    // <-----t----->
    if (beta <= score) { target += delta; }

    delta *= 2;
  }

  ASSERT(0);
  return {};
}

SearchResult Engine::search(int depth) {
  SearchResult res;
  res.depth = depth;

  state->reset();
  res.score = searchImpl(-kScoreInf, kScoreInf, 0, depth, res);
  res.pv = state->pv;
  res.stats_time = time_control.getTime() + 1;
  return res;
}

Score Engine::searchImpl(Score alpha, Score beta, int depth, int depth_end, SearchResult& result) {
  if (!checkSearchLimit()) { return kScoreNone; }
  if (position.isDraw()) { return kScoreDraw; }
  if (depth >= Position::kMaxDepth) { return position.evaluate(); }
  if (depth >= depth_end) { return quiescenceSearch(alpha, beta, depth, result); }

  result.stats_nodes++;
  result.stats_max_depth = std::max(result.stats_max_depth, depth);

  TTEntry tt_entry;
  bool tt_hit = transposition_table.get(position.state->key, tt_entry);
  tt_hit = tt_hit && position.isPseudoLegal(tt_entry.move) && position.isLegal(tt_entry.move);
  result.stats_tt_hit += tt_hit;

  Move best_move = kNoneMove;
  NodeType node_type = kAllNode;
  Score score = -kScoreInf;
  Score evaluation = kScoreNone;

  bool interrupted = 0;
  int depth_to_go = depth_end - depth;
  bool in_check = position.state->checkers;
  Move tt_move = tt_hit ? tt_entry.move : kNoneMove;
  MoveList searched_quiets, searched_captures;
  int move_cnt = 0;
  int searched_move_cnt = 0;

  // Use lambda to skip from anywhere to the end
  ([&]() {

    if (tt_hit) {
      // Hash score cut
      if (depth_to_go <= tt_entry.depth) {
        if (beta <= tt_entry.score && (tt_entry.node_type == kCutNode || tt_entry.node_type == kPVNode)) {
          score = tt_entry.score;
          ASSERT(-kScoreInf < score && score < kScoreInf);
          node_type = kCutNode;
          best_move = tt_move;
          result.stats_tt_cut++;
          return;
        }
        if (tt_entry.score <= alpha && tt_entry.node_type == kAllNode) {
          score = tt_entry.score;
          ASSERT(-kScoreInf < score && score < kScoreInf);
          node_type = kAllNode;
          result.stats_tt_cut++;
          return;
        }
      }

      // Hash evaluation
      evaluation = tt_entry.evaluation;
    }

    // Static evaluation
    if (evaluation == kScoreNone) { evaluation = position.evaluate(); }

    MovePicker move_picker(position, history, tt_move, state->killers, in_check, /* quiescence */ false);
    Move move;
    while (move_picker.getNext(move)) {
      move_cnt++;

      bool is_capture = position.isCaptureOrPromotion(move);
      bool gives_check = position.givesCheck(move);
      Score history_score = is_capture ? history.getCaptureScore(position, move) : history.getQuietScore(position, move);
      (is_capture ? searched_captures : searched_quiets).put(move);

      // Futility pruning
      if (!is_capture && !in_check && !gives_check && depth_to_go <= 3) {
        if (evaluation + 200 * depth_to_go < alpha && history_score < -10) {
          result.stats_futility_prune++;
          continue;
        }
      }

      makeMove(move);

      // Late move reduction
      if (!in_check && !gives_check && depth >= 1 && depth_to_go >= 3 && move_cnt >= 3) {
        int reduction = 0;
        reduction += !is_capture;
        reduction += (history_score < 0);
        reduction += (history_score < -500);
        reduction = std::min(reduction, depth_to_go - 2);
        if (reduction > 0) {
          Score lmr_score = -searchImpl(-(alpha + 1), -alpha, depth + 1, depth_end - reduction, result);
          if (!checkSearchLimit()) { unmakeMove(move); interrupted = 1; return; }

          result.stats_lmr++;
          if (lmr_score <= alpha) {
            result.stats_lmr_success++;
            unmakeMove(move);
            continue;
          }
        }
      }

      searched_move_cnt++;
      score = std::max<Score>(score, -searchImpl(-beta, -alpha, depth + 1, depth_end, result));
      unmakeMove(move);
      if (!checkSearchLimit()) { interrupted = 1; return; }

      if (beta <= score) { // beta cut
        node_type = kCutNode;
        best_move = move;
        result.stats_refutation += (move_picker.stage == kRefutationStage);
        return;
      }
      if (alpha < score) { // pv
        node_type = kPVNode;
        alpha = score;
        best_move = move;
        state->updatePV(move, (state + 1)->pv);
      }
    }

    // All moves pruned
    if (move_cnt > 0 && searched_move_cnt == 0) { score = evaluation; }

    // Checkmate/stalemate
    if (move_cnt == 0) { score = position.evaluateLeaf(depth); }

  })(); // Lambda End

  if (interrupted) { return kScoreNone; }

  ASSERT(-kScoreInf < score && score < kScoreInf);
  tt_entry.node_type = node_type;
  tt_entry.move = best_move;
  tt_entry.score = score;
  tt_entry.evaluation = evaluation;
  tt_entry.depth = depth_to_go;
  transposition_table.put(position.state->key, tt_entry);

  if (node_type == kCutNode) {
    updateKiller(best_move);
    updateHistory(best_move, searched_quiets, searched_captures, depth_to_go);
  }

  return score;
}

Score Engine::quiescenceSearch(Score alpha, Score beta, int depth, SearchResult& result) {
  if (!checkSearchLimit()) { return kScoreNone; }
  if (position.isDraw()) { return kScoreDraw; }

  result.stats_nodes++;
  result.stats_max_depth = std::max(result.stats_max_depth, depth);
  if (depth >= Position::kMaxDepth) { return position.evaluate(); }

  TTEntry tt_entry;
  bool tt_hit = transposition_table.get(position.state->key, tt_entry);
  tt_hit = tt_hit && position.isPseudoLegal(tt_entry.move) && position.isLegal(tt_entry.move);
  result.stats_tt_hit += tt_hit;

  Move best_move = kNoneMove;
  NodeType node_type = kAllNode;
  Score score = -kScoreInf;
  Score evaluation = kScoreNone;

  bool interrupted = 0;
  bool in_check = position.state->checkers;
  Move tt_move = tt_hit ? tt_entry.move : kNoneMove;
  int move_cnt = 0;
  int searched_move_cnt = 0;

  ([&]() {

    if (tt_hit) {
      // Hash score cut
      if (tt_entry.node_type == kCutNode || tt_entry.node_type == kPVNode) {
        if (beta <= tt_entry.score) {
          score = tt_entry.score;
          ASSERT(-kScoreInf < score && score < kScoreInf);
          node_type = kCutNode;
          best_move = tt_entry.move;
          result.stats_tt_cut++;
          return;
        }
      }
      if (tt_entry.node_type == kAllNode) {
        if (tt_entry.score <= alpha) {
          score = tt_entry.score;
          ASSERT(-kScoreInf < score && score < kScoreInf);
          node_type = kAllNode;
          result.stats_tt_cut++;
          return;
        }
      }

      // Hash evaluation
      evaluation = tt_entry.evaluation;
    }

    // Static evaluation
    if (evaluation == kScoreNone) { evaluation = position.evaluate(); }

    // Stand pat beta cut
    score = evaluation;
    if (beta <= score) { node_type = kCutNode; return; }
    if (alpha < score) { alpha = score; }

    MovePicker move_picker(position, history, tt_move, state->killers, in_check, /* quiescence */ true);
    Move move;
    while (move_picker.getNext(move)) {
      move_cnt++;

      bool is_capture = position.isCaptureOrPromotion(move);
      bool gives_check = position.givesCheck(move);

      // Futility pruning
      if (!in_check && !gives_check) {
        Score history_score = is_capture ? history.getCaptureScore(position, move) : history.getQuietScore(position, move);
        Score see_score = position.evaluateMove(move);
        if (evaluation + 100 < alpha && (history_score < -10 || see_score < -100)) {
          result.stats_futility_prune++;
          continue;
        }
      }

      searched_move_cnt++;
      makeMove(move);
      score = std::max<Score>(score, -quiescenceSearch(-beta, -alpha, depth + 1, result));
      unmakeMove(move);

      if (!checkSearchLimit()) { interrupted = 1; return; }

      if (beta < score) {
        node_type = kCutNode;
        best_move = move;
        return;
      }
      if (alpha < score) {
        node_type = kPVNode;
        alpha = score;
      }
    }

    // Checkmate/stalemate
    if (in_check && move_cnt == 0) { score = position.evaluateLeaf(depth); }

  })(); // Lambda End

  if (interrupted) { return kScoreNone; }

  ASSERT(-kScoreInf < score && score < kScoreInf);
  tt_entry.node_type = node_type;
  tt_entry.move = best_move;
  tt_entry.score = score;
  tt_entry.evaluation = evaluation;
  tt_entry.depth = 0;
  transposition_table.put(position.state->key, tt_entry);

  return score;
}

void Engine::updateKiller(const Move& move) {
  auto& [m0, m1] = state->killers;
  if (m0 == move) { return; }
  m1 = move;
  std::swap(m0, m1);
}

void Engine::updateHistory(const Move& best_move, const MoveList& quiets, const MoveList& captures, int depth) {
  const Score kMaxHistoryScore = 2000;

  auto update = [&](Score sign, Score& result) {
    result += sign * (depth * depth);
    result = std::min<Score>(result, +kMaxHistoryScore);
    result = std::max<Score>(result, -kMaxHistoryScore);
  };

  if (position.isCaptureOrPromotion(best_move)) {
    // Increase best capture
    update(+1, history.getCaptureScore(position, best_move));

  } else {
    // Increase best quiet
    update(+1, history.getQuietScore(position, best_move));

    // Decrease all non-best quiets
    for (auto move : quiets) {
      if (move == best_move) { continue; }
      update(-1, history.getQuietScore(position, move));
    }
  }

  // Decrease all non-best captures
  for (auto move : captures) {
    if (move == best_move) { continue; }
    update(-1, history.getCaptureScore(position, move));
  }
}

void Engine::makeMove(const Move& move) {
  if (move == kNoneMove) {
    position.makeNullMove();
  } else {
    position.makeMove(move);
  }
  ASSERT(state < &search_state_stack.back());
  state++;
  state->reset();
}

void Engine::unmakeMove(const Move& move) {
  state--;
  if (move == kNoneMove) {
    position.unmakeNullMove();
  } else {
    position.unmakeMove(move);
  }
}
