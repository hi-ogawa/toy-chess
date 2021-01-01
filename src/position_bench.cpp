#include "position.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

TEST_CASE("Position::perft") {
  precomputation::initializeTables();
  Position pos(kFenInitialPosition);

  BENCHMARK("perft(5)") {
    return pos.perft(5);
  };

  BENCHMARK("perft(6)") {
    return pos.perft(6);
  };
}
