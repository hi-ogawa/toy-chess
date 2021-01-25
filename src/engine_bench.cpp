#include "engine.hpp"
#include <catch2/catch_test_macros.hpp>
#include "timeit.hpp"

TEST_CASE("Engine::go") {
  Engine engine;

  // TODO: Try different hash table size etc..
  // TODO: Collect nice test positions in one place
  // TODO: Print search statistics (time, nodes, tthit, pruning, reduction, etc...)
  vector<string> fens = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
    "2r1r2k/b1qb1pp1/p2p1n1p/Pp2pN2/2P1Pn2/1B1P1N1P/3Q1PP1/R1B1R1K1 b - - 0 1",
    "r1bq1rk1/1p3ppp/5b2/p1pnN2N/3P4/P7/1PP2PPP/R1BQ1RK1 b - - 1 13"
  };

  for (auto fen : fens) {
    SECTION(fen) {
      engine.reset();
      engine.position.initialize(fen);
      INFO(timeit::timeit([&]() {
        engine.go_parameters.depth = 6;
        engine.go(/* blocking */ true);
        auto result = engine.results.back();
        return result;
      }, 1, 2));
      SUCCEED();
    }
  }
}
