#include "evaluator.hpp"
#include "../position.hpp"
#include "../timeit.hpp"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("nn::Evaluator") {
  nn::Evaluator evaluator;
  evaluator.loadEmbeddedWeight();

  Position pos;
  evaluator.initialize(pos);

  SECTION("initialize") {
    INFO(timeit::timeit([&]() {
      evaluator.initialize(pos);
      return evaluator.accumulator[0][0];
    }));
    SUCCEED();
  }

  SECTION("update") {
    INFO(timeit::timeit([&]() {
      evaluator.update(kWhite, kPawn, kE2, -1);
      evaluator.update(kWhite, kPawn, kE4, +1);
      return evaluator.accumulator[0][0];
    }));
    SUCCEED();
  }

  SECTION("update") {
    INFO(timeit::timeit([&]() {
      return evaluator.evaluate();
    }));
    SUCCEED();
  }
}
