#include "engine.hpp"
#include <catch2/catch_test_macros.hpp>
#include "timeit.hpp"

TEST_CASE("Engine::go") {
  Engine engine;

  // TODO: Try different hash table size
  vector<string> fens = {
    kFenInitialPosition,
    "8/3k1p2/3b1p1p/1R6/1p2P1P1/p6P/P1rRK3/8 b - - 5 45"
  };

  for (auto fen : fens) {
    SECTION(fen) {
      engine.reset();
      engine.position.initialize(fen);
      INFO(timeit::timeit([&]() {
        engine.go_parameters.depth = 6;
        engine.go(/* blocking */ true);
        return engine.results.back();
      }, 1, 4));
      SUCCEED();
    }
  }
}
