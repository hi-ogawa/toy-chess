#include "position.hpp"
#include "timeit.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

TEST_CASE("Position::perft") {
  Position pos;

  SECTION("perft-4") {
    INFO(timeit::timeit([&]() {
      return pos.perft(4);
    }));
    SUCCEED();
  }

  SECTION("perft-5") {
    INFO(timeit::timeit([&]() {
      return pos.perft(5);
    }));
    SUCCEED();
  }

  SECTION("perft-6") {
    INFO(timeit::timeit([&]() {
      return pos.perft(6);
    }, 1, 8));
    SUCCEED();
  }
}
