#include "evaluator.hpp"
#include "../position.hpp"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("nn::Evaluator") {
  nn::Evaluator evaluator;
  evaluator.loadEmbeddedWeight();

  Position pos;
  evaluator.initialize(pos);
  CHECK(std::abs(evaluator.evaluate()) < 70);
}
