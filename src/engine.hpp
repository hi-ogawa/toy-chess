#include "position.hpp"

enum SearchResultType { kSearchResultInfo, kSearchResultBestMove, kNoSearchResult };

struct SearchResult {
  SearchResultType type = kNoSearchResult;
  int depth = 0;
  Score score = 0;
  vector<Move> pv;

  void print(std::ostream& ostr = std::cerr) const { ostr << make_tuple(type, depth, score, pv); }
  friend std::ostream& operator<<(std::ostream& ostr, const SearchResult& self) { self.print(ostr); return ostr; }
};

struct Engine {
  bool running = 0; // TODO: Probably it's supposed to be std::atomic
  bool interrupt = 0;
  std::mutex mutex;
  std::condition_variable cv_not_running;

  vector<SearchResult> results; // Result for each depth during iterative deepening
  std::function<void(const SearchResult&)> search_result_callback = [](auto){};

  struct SearchState {
    array<Move, Position::kMaxDepth> pv;
  };
  SearchState* search_state = nullptr;
  array<SearchState, Position::kMaxDepth> search_state_stack;

  // "go" and "stop" should be called from different threads
  void stop();

  // Iterative deepening
  void go(Position&, int);

  // Fixed depth alph-beta search
  SearchResult search(Position&, int);
  Score searchImpl(Position&, Score, Score, int, int);
};
