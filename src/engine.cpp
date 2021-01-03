#include "engine.hpp"

void Engine::stop() {
  if (!running) { return; }
  assert(!interrupt);
  interrupt = 1;
  std::unique_lock lock(mutex);
  cv_not_running.wait(lock, [&](){ return !running; });
  interrupt = 0;
}

void Engine::go(Position& pos, int depth_end) {
  assert(!running);
  assert(!interrupt);
  assert(depth_end >= 1);

  running = 1;
  results.assign(depth_end + 1, {});

  // Construct result for depth = 1 so that there always is "bestmove" result.
  int last_depth = 1;
  results[1] = search(pos, 1);
  results[1].type = kSearchResultInfo;
  search_result_callback(results[1]);

  // Iterate depening until "interrupt" or reaching "depth_end"
  for (int depth = 2; depth <= depth_end; depth++) {
    auto res = search(pos, depth);
    if (interrupt) { break; } // Ignore possibly incomplete result

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

  // Notify "not_running"
  assert(running);
  running = 0;
  cv_not_running.notify_one();
}

SearchResult Engine::search(Position& pos, int depth) {
  SearchResult res;
  res.depth = depth;

  search_state = &search_state_stack[0];
  res.score = searchImpl(pos, -kScoreInf, kScoreInf, 0, depth);
  if (interrupt) { return res; }

  res.pv.resize(depth);
  for (int i = 0; i < depth; i++) { res.pv[i] = search_state->pv[i]; }

  return res;
}

Score Engine::searchImpl(Position& pos, Score alpha, Score beta, int depth, int depth_end) {
  if (interrupt) { return 0; }

  if (depth == depth_end) { return pos.evaluate(); }
  Move best_move;

  // Children nodes
  pos.generateMoves();
  while (auto move = pos.move_list->getNext()) {
    pos.makeMove(*move);
    search_state++;
    auto score = -searchImpl(pos, -beta, -alpha, depth + 1, depth_end);
    search_state--;
    pos.unmakeMove(*move);
    if (beta <= score) { return score; } // Beta cut
    if (alpha < score) {
      alpha = score;
      best_move = *move;
      for (int d = depth + 1; d < depth_end; d++) {
        search_state->pv[d] = (search_state + 1)->pv[d];
      }
    }
  }

  // Leaf node (checkmate or stalemate)
  if (pos.move_list->size() == 0) {
    auto score = pos.state->checkers ? -Evaluation::mateScore(depth) : kScoreDraw;
    if (beta <= score) { return score; } // Beta cut
    if (alpha < score) {
      alpha = score;
      best_move.type = kNoMoveType;
    }
  }

  search_state->pv[depth] = best_move;
  return alpha;
}
