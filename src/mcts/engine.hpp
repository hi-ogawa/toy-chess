#include "../position.hpp"
#include "../engine.hpp"
#include "utils.hpp"

namespace mcts {

struct Node;

struct Edge {
  Node* to = nullptr;
  Move move = kNoneMove;
  float p = 0;

  void print(std::ostream& ostr) const;
  friend std::ostream& operator<<(std::ostream& ostr, const Edge& self) { self.print(ostr); return ostr; }
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
  int64_t size = 16 * (1 << 20);

  Tree() { reset(); }

  void reset() {
    nodes.assign(size, {});
    edges.assign(size, {});
    node_cnt = 1;
    edge_cnt = 0;
    visit_cnt = 0;
    nodes[0] = {};
  }

  Node& getRoot() { return nodes[0]; }

  Node& emplaceNode() {
    ASSERT(node_cnt < size);
    return nodes[node_cnt++];
  }

  Edge& emplaceEdge() {
    ASSERT(edge_cnt < size);
    return edges[edge_cnt++];
  }

  Edge* getEdgeEnd() { return (&edges[0]) + edge_cnt; }

  bool checkLimit() {
    return std::min(node_cnt, edge_cnt) <= size - 200;
  }
};

struct Engine : ::EngineBase {
  nn::Evaluator evaluator;
  nn::MoveEvaluator move_evaluator;
  Tree tree;
  float cpuct = 1;
  int64_t report_interval = 500; // in msec
  int64_t next_report_time = 0;

  Engine();

  // Override
  string getFen() { return position.toFen(); }
  void setFen(const string& fen) { position.initialize(fen); }
  void reset() { setFen(kFenInitialPosition); }
  void print(std::ostream&);
  void goImpl();
  void setCPUCT(float new_cpuct) { cpuct = new_cpuct; }

  // MCTS search
  float search(Node&, int);
  void initializeEdges(Node&);
  bool checkSearchLimit();
  void reportSearchResult();
  SearchResult makeSearchResult();
};


}; // namespace mcts
