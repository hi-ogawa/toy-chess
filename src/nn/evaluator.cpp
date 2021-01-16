#include "evaluator.hpp"
#include "../position.hpp"

namespace nn {

Score Evaluator::evaluate() {
  relu<2 * WIDTH2>(accumulator[0], tmp2);
  model.l2->forward(tmp2, tmp3);
  relu<WIDTH3>(tmp3, tmp3);
  model.l3->forward(tmp3, tmp4);
  relu<WIDTH4>(tmp4, tmp4);
  model.l4->forward(tmp4, &tmp5);
  Score score = std::round(tmp5 * 100);
  return std::clamp<Score>(score, -kScoreWin, kScoreWin);
}

void Evaluator::initialize(const Position& pos) {
  copy<WIDTH2>(model.l1->bias, accumulator[0]);
  copy<WIDTH2>(model.l1->bias, accumulator[1]);

  kings[0] = pos.kingSQ(kWhite);
  kings[1] = SQ::flipRank(pos.kingSQ(kBlack));

  for (Color color = 0; color < 2; color++) {
    for (PieceType type = 0; type < 5; type++) {
      for (auto sq : toSQ(pos.pieces[color][type])) {
        putPiece(color, type, sq);
      }
    }
  }
}

void Evaluator::update(Color color, PieceType type, Square sq, bool put) {
  if (type == kKing) { return; }
  int type_w = type + 5 * (color == 1);
  int type_b = type + 5 * (color == 0);
  int sq_w = sq;
  int sq_b = SQ::flipRank(sq);
  int index_w = (type_w * 64 + sq_w) * 64 + kings[0];
  int index_b = (type_b * 64 + sq_b) * 64 + kings[1];
  if (put) {
    add<WIDTH2>(accumulator[0], model.l1->weight[index_w], accumulator[0]);
    add<WIDTH2>(accumulator[1], model.l1->weight[index_b], accumulator[1]);
  } else {
    sub<WIDTH2>(accumulator[0], model.l1->weight[index_w], accumulator[0]);
    sub<WIDTH2>(accumulator[1], model.l1->weight[index_b], accumulator[1]);
  }
}

}; // namespace nn
