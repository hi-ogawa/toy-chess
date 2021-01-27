#include "move_evaluator.hpp"
#include "../position.hpp"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("nn::MoveEvaluator") {
  nn::MoveEvaluator move_evaluator;
  move_evaluator.loadEmbeddedWeight();

  Position pos;
  move_evaluator.initialize(pos);

  MoveList moves;
  pos.generateMoves(moves);

  SimpleQueue<std::pair<Move, float>, 256> ls;
  float sum = 0;
  for (auto move : moves) {
    float score = move_evaluator.evaluate(pos.side_to_move, move.from(), move.to());
    score = std::exp(score);
    sum += score;
    ls.put({move, score});
  }
  for (auto& [move, score] : ls) { score /= sum; }
  std::sort(ls.begin(), ls.end(), [](auto x, auto y) { return x.second > y.second; });

  CHECK(toString(ls[0].first) == "b1c3");
}
