#include "engine.hpp"

namespace mcts {

//
// Edge
//

void Edge::print(std::ostream& ostr) const {
  ostr << std::make_tuple(move, to->n, p, -to->q);
}


//
// Engine
//

Engine::Engine() {
  evaluator.loadEmbeddedWeight();
  move_evaluator.loadEmbeddedWeight();
  position.evaluator = &evaluator;
  position.move_evaluator = &move_evaluator;
  position.reset();
}

void Engine::print(std::ostream& ostr) {
  ostr << ":: Position" << "\n";
  ostr << position;
  ostr << ":: Evaluation" << "\n";
  ostr << evaluator.evaluate() << "\n";
};

bool Engine::checkSearchLimit() {
  if (tree.node_cnt < 256) { return 1; } // Search at least "depth 1"
  if (stop_requested.load(std::memory_order_acquire)) { return 0; }
  if (!time_control.checkLimit()) { return 0; }
  if (!tree.checkLimit()) { return 0; }
  return 1;
}

void Engine::goImpl() {
  tree.reset();
  next_report_time = 0;
  auto& root = tree.getRoot();
  for (int i = 0; i < 2; i++) { search(root, 0); } // Search twice to get "depth 1"
  for (int i = 0; ; i++) {
    if (!checkSearchLimit()) { break; }
    search(root, 0);
    if (time_control.getTime() > next_report_time) {
      next_report_time = time_control.getTime() + report_interval;
      reportSearchResult();
    }
  }
  reportSearchResult();
  auto best = makeSearchResult();
  best.type = kSearchResultBestMove;
  search_result_callback(best);
}

void Engine::reportSearchResult() {
  auto result = makeSearchResult();
  result.type = kSearchResultInfo;
  search_result_callback(result);
  if (debug) {
    SearchResult info;
    info.type = kSearchResultInfo;
    auto& root = tree.getRoot();
    info.debug = toString(root.n, root.getTopK(3));
    search_result_callback(info);
  }
}

SearchResult Engine::makeSearchResult() {
  SearchResult result;
  result.type = kSearchResultInfo;
  result.stats_time = time_control.getTime();
  result.stats_nodes = tree.visit_cnt;
  auto& root = tree.getRoot();
  result.score = reward2cp(root.q);
  root.getPV(result.pv);
  result.depth = result.pv.size();
  return result;
}

float Engine::search(Node& node, int depth) {
  if (!checkSearchLimit()) { return 0; }

  node.n++;
  if (node.is_terminal) { return node.q; }

  if (position.isDraw()) {
    tree.visit_cnt++;
    node.is_terminal = 1;
    return node.q = 0;
  }

  // On first visit, initialize reward only
  if (node.n == 1) {
    tree.visit_cnt++;
    Score score = position.evaluate();
    return node.q = cp2reward(score);
  }

  // Back off when too deep
  if (depth >= Position::kMaxDepth) { return node.q; }

  // On second visit, initialize policy
  if (node.n == 2) {
    initializeEdges(node);

    // Finally detect checkmate/stalemate
    if (node.num_edges == 0) {
      node.is_terminal = 1;
      return node.q = position.inCheck() ? -1 : 0;
    }
  }

  // Find max upper confidence bound
  auto ucb = [&](const Edge& e) -> float {
    float x = -e.to->q;
    float y = e.p * std::sqrt(node.n) / (e.to->n + 1e-3);
    return x + cpuct * y;
  };
  Edge best = maxElementByKey(node.getEdges(), ucb);

  // Search recursively
  position.makeMove(best.move);
  float q = -search(*best.to, depth + 1);
  position.unmakeMove(best.move);
  if (!checkSearchLimit()) { return 0; }

  // Update reward
  node.q = (node.n * node.q + q) / (node.n + 1);

  return q;
}

void Engine::initializeEdges(Node& node) {
  float sum = 0;
  node.edge_begin = tree.getEdgeEnd(); // Point to global edge buffer

  MoveList list;
  position.generateMoves(list);
  for (auto move : list) {
    if (!position.isLegal(move)) { continue; }
    if (move.isPromotionBR()) { continue; } // Ignore bishop/rook promotion

    float score = move_evaluator.evaluate(position.side_to_move, move.from(), move.to());
    score = std::exp(score);
    sum += score;
    node.num_edges++;

    // Allocate node and edge
    tree.emplaceEdge() = {&tree.emplaceNode(), move, score};
  }

  // Normalize
  for (auto& e : node.getEdges()) { e.p /= sum; }
}

} // namespace mcts
