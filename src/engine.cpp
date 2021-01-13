#include "engine.hpp"

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
  time_control.initialize(go_parameters, position.side_to_move);
  ASSERT(checkSearchLimit());

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

  search_state = &search_state_stack[0];
  res.score = searchImpl(-kScoreInf, kScoreInf, 0, depth);

  res.pv.resize(depth);
  for (int i = 0; i < depth; i++) {
    res.pv[i] = search_state->pv[i];
    if (res.pv[i].type == kNoMoveType) { break; }
  }

  return res;
}

Score Engine::searchImpl(Score alpha, Score beta, int depth, int depth_end) {
  if (!checkSearchLimit()) { return 0; }

  if (depth == depth_end) { return position.evaluate(); }
  Move best_move;

  // Children nodes
  position.generateMoves();
  while (auto move = position.move_list->getNext()) {
    position.makeMove(*move);
    search_state++;
    auto score = -searchImpl(-beta, -alpha, depth + 1, depth_end);
    search_state--;
    position.unmakeMove(*move);
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
  if (position.move_list->size() == 0) {
    auto score = position.state->checkers ? -Evaluation::mateScore(depth) : kScoreDraw;
    if (beta <= score) { return score; } // Beta cut
    if (alpha < score) {
      alpha = score;
      best_move.type = kNoMoveType;
    }
  }

  search_state->pv[depth] = best_move;
  return alpha;
}
