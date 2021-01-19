#include "engine.hpp"

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
  }
  finish = start + Msec(int64_t(kSafeFactor * duration));
};

void SearchResult::print(std::ostream& ostr) const {
  if (type == kSearchResultInfo) {
    if (!misc.empty()) {
      ostr << "info string " << misc;

    } else {
      int64_t nps = (1000 * num_nodes) / time;
      ostr << "info"
           << " depth " << depth
           << " score cp " << score
           << " time " << time
           << " nodes " << num_nodes
           << " nps " << nps
           << " pv";
      for (auto move : pv) {
        if (move.type() == kNoMoveType) { break; }
        ostr << " " << move;
      }
    }
  }

  if (type == kSearchResultBestMove) {
    ostr << "bestmove " << pv[0];
  }
}

void Engine::print(std::ostream& ostr) {
  ostr << ":: Position" << "\n";
  ostr << position;
  ostr << ":: Evaluation" << "\n";
  ostr << evaluator.evaluate() << "\n";
}

void Engine::stop() {
  if (!isRunning()) { return; }
  ASSERT(!stop_requested.load(std::memory_order_acquire));
  stop_requested.store(true, std::memory_order_release);
  wait();
  stop_requested.store(false, std::memory_order_release);
}

void Engine::wait() {
  ASSERT(go_thread_future.valid()); // Check Engine::go didn't meet with Engine::wait yet
  go_thread_future.wait();
  ASSERT(go_thread_future.get());
  go_thread_future = {}; // Invalidate future
}

bool Engine::checkSearchLimit() {
  if (stop_requested.load(std::memory_order_acquire)) { return 0; }
  if (!time_control.checkLimit()) { return 0; }
  return 1;
}

void Engine::go(bool blocking) {
  ASSERT(!go_thread_future.valid()); // Check previous Engine::go met with Engine::wait
  go_thread_future = std::async([&]() { goImpl(); return true; });
  if (blocking) { wait(); }
}

void Engine::goImpl() {
  time_control.initialize(go_parameters, position.side_to_move, position.game_ply);
  ASSERT(checkSearchLimit());

  // Debug info
  SearchResult info;
  info.type = kSearchResultInfo;
  info.misc += "side = " + toString(position.side_to_move) + ", ";
  info.misc += "eval = " + toString(evaluator.evaluate()) + ", ";
  info.misc += "time = " + toString(time_control.getDuration());
  search_result_callback(info);

  int depth_end = go_parameters.depth;
  ASSERT(depth_end > 0);
  results.assign(depth_end + 1, {});

  // Construct result for depth = 1 so that there always is "bestmove" result.
  int last_depth = 1;
  results[1] = search(1);
  results[1].type = kSearchResultInfo;
  search_result_callback(results[1]);

  // Iterative deepening
  for (int depth = 2; depth <= depth_end; depth++) {
    auto res = search(depth);
    if (!checkSearchLimit()) { break; } // Ignore possibly incomplete result

    // Save result and send "info ..."
    last_depth = depth;
    results[depth] = res;
    results[depth].type = kSearchResultInfo;
    results.push_back(results[depth]);
    search_result_callback(results[depth]);
  }

  // Send "bestmove ..."
  results[last_depth].type = kSearchResultBestMove;
  search_result_callback(results[last_depth]);
}

SearchResult Engine::search(int depth) {
  SearchResult res;
  res.depth = depth;

  state = &search_state_stack[0];
  res.score = searchImpl(-kScoreInf, kScoreInf, 0, depth, res);
  res.time = time_control.getTime() + 1;

  res.pv.resize(depth);
  for (int i = 0; i < depth; i++) {
    res.pv[i] = state->pv[i];
    if (res.pv[i].type() == kNoMoveType) { break; }
  }

  return res;
}

Score Engine::quiescenceSearch(Score alpha, Score beta, int depth, SearchResult& result) {
  if (!checkSearchLimit()) { return position.evaluate(); }
  result.num_nodes++;

  auto& tt_entry = transposition_table.probe(position.state->key);

  Move best_move = kNoneMove;
  NodeType node_type = kAllNode;
  Score score = kNoneScore;
  Score evaluation = kNoneScore;

  ([&]() {

    // NOTE: this blindly ignores key collision
    if (tt_entry.hit) {
      // Hash score beta cut
      if (tt_entry.node_type != kAllNode) {
        if (beta <= tt_entry.score && position.isPseudoLegal(tt_entry.move) && position.isLegal(tt_entry.move)) {
          score = tt_entry.score;
          node_type = kCutNode;
          best_move = tt_entry.move;
          return;
        }
      }

      // Hash evaluation
      score = evaluation = tt_entry.evaluation;
    }

    // Static evaluation
    if (score == kNoneScore) { score = evaluation = position.evaluate(); }

    // Stand pat beta cut
    if (beta <= score) {
      node_type = kCutNode;
      return;
    }
    if (alpha < score) {
      node_type = kPVNode;
      alpha = score;
    }

    // Recursively search capture/promotion moves
    generateMoves(tt_entry, /* quiescence */ 1);
    while (auto move = getNextMove()) {
      if (!position.isLegal(move)) { continue; }

      position.makeMove(move);
      state++;
      score = std::max<Score>(score, -quiescenceSearch(-beta, -alpha, depth + 1, result));
      state--;
      position.unmakeMove(move);
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

    // TODO: Handle checkmate/stalemate within quiescence search.
    //       Problem is that currently above move generation doesn't include evasions.

  })();

  ASSERT(-kScoreInf < score && score < kScoreInf);
  tt_entry.hit = 1;
  tt_entry.key = position.state->key;
  tt_entry.node_type = node_type;
  tt_entry.move = best_move;
  tt_entry.score = score;
  tt_entry.evaluation = evaluation;
  tt_entry.depth = 0;
  return score;
}

Score Engine::searchImpl(Score alpha, Score beta, int depth, int depth_end, SearchResult& result) {
  if (!checkSearchLimit()) { return position.evaluate(); }

  if (depth == depth_end) { return quiescenceSearch(alpha, beta, 0, result); }

  result.num_nodes++;

  // NOTE: possible hit entry can be overwritten during recursive call of "searchImpl"
  auto& tt_entry = transposition_table.probe(position.state->key);

  Move best_move = kNoneMove; // None move indicates checkmate/stalemate
  NodeType node_type = kAllNode;
  Score score = -kScoreInf;
  int depth_to_go = depth_end - depth;

  // Use lambda to skip from anywhere to the end
  ([&]() {

    // Hash score beta cut (NOTE: this blindly ignores key collision)
    if (tt_entry.hit && depth_to_go <= tt_entry.depth && tt_entry.node_type != kAllNode) {
      if (beta <= tt_entry.score && position.isPseudoLegal(tt_entry.move) && position.isLegal(tt_entry.move)) {
        score = tt_entry.score;
        node_type = kCutNode;
        best_move = tt_entry.move;
        updateKiller(tt_entry.move);
        return;
      }
    }

    int move_cnt = 0;
    generateMoves(tt_entry);
    while (auto move = getNextMove()) {
      if (!position.isLegal(move)) { continue; }
      move_cnt++;

      position.makeMove(move);
      state++;
      score = std::max<Score>(score, -searchImpl(-beta, -alpha, depth + 1, depth_end, result));
      state--;
      position.unmakeMove(move);
      if (beta <= score) { // beta cut
        node_type = kCutNode;
        best_move = move;
        updateKiller(move);
        return;
      }
      if (alpha < score) { // pv
        node_type = kPVNode;
        alpha = score;
        best_move = move;
        for (int d = depth + 1; d < depth_end; d++) {
          state->pv[d] = (state + 1)->pv[d];
        }
      }
    }

    if (move_cnt == 0) {
      // Leaf node (checkmate or stalemate)
      score = std::max<Score>(score, position.evaluateLeaf(depth));
      if (beta <= score) {
        node_type = kCutNode;
        return;
      }
      if (alpha < score) {
        node_type = kPVNode;
      }
    }
  })(); // lambda end

  ASSERT(-kScoreInf < score && score < kScoreInf);
  tt_entry.hit = 1;
  tt_entry.key = position.state->key;
  tt_entry.node_type = node_type;
  tt_entry.move = best_move;
  tt_entry.score = score;
  tt_entry.depth = depth_to_go;

  // Update pv
  if (node_type == kPVNode) { state->pv[depth] = best_move; }

  return score;
}

void Engine::generateMoves(const TTEntry& tt_entry, bool quiescence) {
  state->move_list.clear();
  state->move_generators.clear();

  if (!quiescence) {
    // Hash move
    // TODO: There seems some tt related bug only in debug mode (which happens around checkmate/stalemate)
    if (kBuildType == "Release" && tt_entry.hit && tt_entry.node_type != kAllNode) {
      auto move = tt_entry.move;
      if (position.isPseudoLegal(move)) {
        state->move_generators.put([&](auto& ls) { ls.put({move, kHashScore}); });
      }
    }
  }


  // Capture/Promotion moves
  // TODO: Don't repeat hash move
  // TODO: Postpone bad captures after killer and quiet
  // TODO: Score promotion properly
  state->move_generators.put([&](auto& ls) {
    MoveList ls_tmp;
    position.generateMoves(ls_tmp, kGenerateCapture);
    for (auto& [move, score] : ls_tmp) {
      // TODO: Avoid duplicated legality check in "searchImpl" and "quiescenceSearch"
      if (!position.isLegal(move)) { continue; }

      // Assign SEE score
      Score see_score = position.evaluateCapture(move);
      if (quiescence && see_score <= 0) { continue; } // Skip bad SEE in quiescence

      ls.put({move, see_score});
    }

    // Sort by SEE score
    std::sort(ls.begin(), ls.end(), [](auto x, auto y) { return x.second > y.second; });
  });

  // Killer moves
  if (!quiescence) {
    state->move_generators.put([&](auto& ls) {
      for (auto move : state->killers) {
        if (!position.isCapture(move) && position.isPseudoLegal(move)) {
          ls.put({move, kKillerScore});
        }
      }
    });
  }

  // Quiet moves
  if (!quiescence) {
    state->move_generators.put([&](auto& ls) {
      position.generateMoves(ls, kGenerateQuiet);
      std::sort(ls.begin(), ls.end(), [](auto x, auto y) { return x.second < y.second; });
    });
  }
}

Move Engine::getNextMove() {
  while (state->move_list.empty()) {
    if (state->move_generators.empty()) { return kNoneMove; }
    state->move_generators.get()(state->move_list);
  }
  return state->move_list.get().first;
}

void Engine::updateKiller(const Move& move) {
  auto& [m0, m1] = state->killers;
  if (m0 == move) { return; }
  m1 = move;
  std::swap(m0, m1);
}
