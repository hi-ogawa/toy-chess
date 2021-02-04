#include "../position.hpp"
#include "evaluator.hpp"

namespace zero {

struct Node;

struct Edge {
  Node* to = nullptr;
  Move move = kNoneMove;
  float p = 0;
};

struct Node {
  int n = 0;
  float q = 0;
  int num_edges = 0;
  Edge* edge_begin = nullptr;
  bool is_terminal = 0;

  PointerIterator<Edge> getEdges() { return {edge_begin, edge_begin + num_edges}; }

  void getPV(MoveList& moves) {
    if (num_edges == 0) { return; }
    Edge best = maxElementByKey(getEdges(), [](auto e) { return e.to->n; });
    moves.put(best.move);
    best.to->getPV(moves);
  }

  vector<Edge> getTopK(int size = -1) {
    vector<Edge> res = {getEdges().begin(), getEdges().end()};
    std::sort(res.begin(), res.end(), [](auto x, auto y) { return x.to->n > y.to->n; });
    if (size != -1) { res.erase(res.begin() + std::min(size, num_edges), res.end()); }
    return res;
  }
};

static_assert(sizeof(Edge) == 16);
static_assert(sizeof(Node) == 32);


struct Tree {
  // All nodes and edges are pre-allocated
  vector<Node> nodes;
  vector<Edge> edges;
  int64_t node_cnt = 1;
  int64_t edge_cnt = 0;
  int64_t visit_cnt = 0; // Count visited node (i.e. evaluated position)
  int64_t size = 100000;

  Tree() { reset(); }

  void reset() {
    nodes.assign(size, {});
    edges.assign(size, {});
    node_cnt = 1;
    edge_cnt = 0;
    visit_cnt = 0;
    nodes[0] = {};
  }

  Node& root() { return nodes[0]; }

  Node& emplaceNode() {
    ASSERT(node_cnt < size);
    return nodes[node_cnt++];
  }

  Edge& emplaceEdge() {
    ASSERT(edge_cnt < size);
    return edges[edge_cnt++];
  }

  Edge* getEdgeEnd() { return &edges[0] + edge_cnt; }
};

inline float logit2reward(float logit) { return std::tanh(logit); }

struct Engine {
  Position position;
  Evaluator evaluator;
  Tree tree;
  float cpuct = 1;

  Engine() {
    position.zero_evaluator = &evaluator;
  }

  void searchMain(const string& fen, int n) {
    position.initialize(fen);
    tree.reset();
    for (int i = 0; i < n; i++) { search(tree.root()); }
  }

  float search(Node& node, int depth = 0) {
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
      float logit = evaluator.computeWDL(position.side_to_move);
      return node.q = logit2reward(logit);
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

    // Find a child node with maximum upper confidence bound
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

    // Update reward
    node.q = (node.n * node.q + q) / (node.n + 1);

    return q;
  }

  void initializeEdges(Node& node) {
    float sum = 0;
    node.edge_begin = tree.getEdgeEnd(); // Point to global edge buffer

    MoveList list;
    position.generateMoves(list);
    for (auto move : list) {
      if (!position.isLegal(move)) { continue; }
      if (move.type() == kPromotion && move.type() != kQueen) { continue; } // TODO: only Queen promotion for now ...

      float score = evaluator.computePolicy(position.side_to_move, move.from(), move.to());
      score = std::exp(score);
      sum += score;
      node.num_edges++;

      // Allocate node and edge
      tree.emplaceEdge() = {&tree.emplaceNode(), move, score};
    }

    // Normalize
    for (auto& e : node.getEdges()) { e.p /= sum; }
  }
};

struct __attribute__ ((packed)) Entry {
  array<uint16_t, 64> halfkp = {};
  array<uint16_t, POLICY_TOP_K> topk_indices = {}; // NOTE: dummy index is not necessary for the case when there's no "k" moves
  array<float, POLICY_TOP_K> topk_values = {};
  int8_t wdl = 0;
};
static_assert(sizeof(Entry) == 2 * 64 + (2 + 4) * POLICY_TOP_K + 1);

struct Selfplay {
  string weight_file;
  string output_file;
  int num_episodes = 8;
  int num_simulations = 8;
  int log_level = 0;
  bool exclude_draw = 0;
  Rng rng;

  Engine engine;
  std::ofstream ostr;

  void run() {
    ostr.open(output_file);
    engine.evaluator.load(weight_file);
    for (int i = 0; i < num_episodes; ) {
      if (log_level > 0) { ::print("episode", i); }
      i += runEpisode();
    }
  }

  bool runEpisode() {
    Position position;
    vector<Entry> episode;

    for (int i = 0; ;i++) {
      if (log_level > 1 && i % 25 == 0) { ::print("ply", i); }

      // Run MCTS
      engine.searchMain(position.toFen(), num_simulations);
      auto& root = engine.tree.root();

      // Game finished
      if (root.is_terminal) {
        ASSERT(root.q == 0 || root.q == -1);
        int wdl = root.q;
        if (i % 2 == 1) { wdl *= -1; }
        if (log_level > 0) { ::print("wdl", wdl); }
        if (log_level > 2) { position.print(); }
        if (exclude_draw && wdl == 0) { return 0; }
        if (root.q == -1) {
          for (auto& entry : episode) {
            entry.wdl = wdl;
            wdl *= -1;
          }
        }
        break;
      }

      // Make training data entry
      auto& entry = episode.emplace_back();
      const uint16_t kEmbeddingPad = 10 * 64 * 64;
      std::fill(&entry.halfkp[0], &entry.halfkp[64], kEmbeddingPad);
      uint16_t* x_w = &entry.halfkp[0];
      uint16_t* x_b = &entry.halfkp[32];
      if (position.side_to_move == kBlack) { std::swap(x_w, x_b); }
      position.writeHalfKP(x_w, x_b);

      auto topk = root.getTopK(POLICY_TOP_K);
      float sum = 0;
      for (int j = 0; j < (int)topk.size(); j++) {
        Square from = topk[j].move.from();
        Square to = topk[j].move.to();
        if (position.side_to_move == kBlack) {
          from = SQ::flipRank(from);
          to = SQ::flipRank(to);
        }
        entry.topk_indices[j] = precomputation::encode_from_to[from][to];
        entry.topk_values[j] = topk[j].to->n;
        sum += topk[j].to->n;
      }
      for (int j = 0; j < (int)topk.size(); j++) {
        entry.topk_values[j] /= sum;
      }

      // Sample move and continue
      Move move = sampleMove(topk, sum);
      position.makeMove(move);
      position.reset();
    }

    // Write data
    if (log_level > 0) { ::print("Writing entries", episode.size()); }
    ostr.write((char*)episode.data(), sizeof(episode[0]) * episode.size());
    return 1;
  }

  Move sampleMove(const vector<Edge>& edges, float sum) {
    // Invert cumulative distribution
    float x = sum * rng.uniform();
    float y = 0;
    for (auto& edge : edges) {
      y += edge.to->n;
      if (x <= y) { return edge.move; }
    }
    ASSERT(0);
    return kNoneMove;
  }
};

} // namespace zero
