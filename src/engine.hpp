#pragma once

#include "position.hpp"
#include "transposition_table.hpp"
#include "nn/evaluator.hpp"

enum SearchResultType { kSearchResultInfo, kSearchResultBestMove, kNoSearchResult };

struct SearchResult {
  SearchResultType type = kNoSearchResult;
  int depth = 0;
  Score score = 0;
  int64_t stats_time = 0;
  int64_t stats_nodes = 0;
  int64_t stats_tt_hit = 0;
  int64_t stats_tt_cut = 0;
  int64_t stats_refutation = 0;
  int64_t stats_futility_prune = 0;
  int64_t stats_lmr = 0;
  int64_t stats_lmr_success = 0;
  int stats_aspiration = -1;
  int stats_max_depth = 0;
  MoveList pv;
  string debug;

  void print(std::ostream& ostr = std::cerr) const;
  friend std::ostream& operator<<(std::ostream& ostr, const SearchResult& self) { self.print(ostr); return ostr; }
};


struct GoParameters {
  // Time unit is millisecond
  array<int64_t, 2> time = {0, 0};
  array<int64_t, 2> inc = {0, 0};
  int64_t movestogo = 0;
  int64_t movetime = 0;
  int depth = Position::kMaxDepth;
};


struct TimeControl {
  using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;
  using Msec = std::chrono::milliseconds;

  static inline TimePoint now() { return std::chrono::steady_clock::now(); }
  static inline const double kSafeFactor = 0.9;
  static inline const double kInfDuration = 1e12; // 10^12 msec ~ 30 years

  TimePoint start, finish;

  void initialize(const GoParameters&, Color, int);
  bool checkLimit() { return now() < finish; }
  int64_t getTime() { return std::chrono::duration_cast<Msec>(now() - start).count(); }
  int64_t getDuration() { return std::chrono::duration_cast<Msec>(finish - start).count(); }
};


struct SearchState {
  MoveList pv;
  array<Move, 2> killers = {};

  void updatePV(const Move& move, const MoveList& child_pv) {
    pv.clear();
    pv.put(move);
    for (auto child : child_pv) { pv.put(child); }
  }

  void reset() {
    pv.clear();
    (this + 1)->killers = {};
  }
};

struct History {
  // For quiet moves (color, from, to)
  array3<Score, 2, 64, 64> quiet = {};

  // For capture (color, attacker, to, attackee) (includes promotion/enpassant)
  array4<Score, 2, 6, 64, 7> capture = {};

  Score& getQuietScore(const Position& p, const Move& move) {
    Color own = p.side_to_move;
    return quiet[own][move.from()][move.to()];
  }

  Score& getCaptureScore(const Position& p, const Move& move) {
    Color own = p.side_to_move;
    PieceType attacker = p.piece_on[ own][move.from()];
    PieceType attackee = p.piece_on[!own][move.to()];
    return capture[own][attacker][move.to()][attackee];
  }
};


struct EngineBase {
  Position position;
  GoParameters go_parameters = {};
  TimeControl time_control = {};

  std::atomic<bool> debug = 0;
  std::atomic<bool> stop_requested = 0; // single reader ("go" thread) + single writer ("stop" thread)
  std::future<bool> go_thread_future;
  std::function<void(const SearchResult&)> search_result_callback = [](const SearchResult&){};

  void stop();
  void wait();
  void go(bool blocking);

  virtual ~EngineBase() {};
  virtual string getFen() { ASSERT(0); return ""; };
  virtual void setFen(const string&) {};
  virtual void reset() {};
  virtual void print(std::ostream&) {};
  virtual void goImpl() {};

  // TODO: These are out of place...
  virtual void setHashSizeMB(int) {};
  virtual void loadWeight(const string&) {};
  virtual void setCPUCT(float) {}
};


struct Engine : EngineBase {
  nn::Evaluator evaluator;
  History history;
  TranspositionTable transposition_table;
  SearchState* state = nullptr;
  array<SearchState, Position::kMaxDepth + 64> search_state_stack;
  vector<SearchResult> results; // Result for each depth during iterative deepening

  static inline const int kDefaultHashSizeMB = 128;
  static inline const string kEmbeddedWeightName = "__EMBEDDED_WEIGHT__";

  Engine() {
    evaluator.loadEmbeddedWeight();
    position.evaluator = &evaluator;
    setHashSizeMB(kDefaultHashSizeMB);
    state = &search_state_stack[0];
    reset();
  }

  string getFen() { return position.toFen(); }
  void setFen(const string& fen) { position.initialize(fen); }

  void reset() {
    position.initialize(kFenInitialPosition);
    transposition_table.reset();
    history = {};
  }

  // Iterative deepening
  void goImpl();

  bool checkSearchLimit();

  // Fixed depth alph-beta search (NOTE: Only usable from "go" method)
  SearchResult search(int);
  SearchResult searchWithAspirationWindow(int, Score);
  Score searchImpl(Score, Score, int, int, SearchResult&);
  Score quiescenceSearch(Score, Score, int, SearchResult&);

  void makeMove(const Move& move);
  void unmakeMove(const Move& move);
  void updateKiller(const Move&);
  void updateHistory(const Move&, const MoveList&, const MoveList&, int);

  // Misc
  void print(std::ostream& ostr = std::cerr);

  void setHashSizeMB(int mb) { transposition_table.resize(mb); }

  void loadWeight(const string& filename = kEmbeddedWeightName) {
    if (filename == kEmbeddedWeightName) evaluator.loadEmbeddedWeight();
    else evaluator.load(filename);
  }
};
