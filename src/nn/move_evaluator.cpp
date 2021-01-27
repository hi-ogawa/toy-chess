#include "move_evaluator.hpp"
#include "../position.hpp"
#include "../precomputation.hpp"

namespace nn {

float MoveEvaluator::evaluate(Color color, Square from, Square to) {
  alignas(kMaxFloatVectorSize) float tmp[2][WIDTH2] = {};
  if (color == kWhite) {
    relu<WIDTH2>(accumulator[0], tmp[0]);
    relu<WIDTH2>(accumulator[1], tmp[1]);
  } else {
    relu<WIDTH2>(accumulator[0], tmp[1]);
    relu<WIDTH2>(accumulator[1], tmp[0]);
    from = SQ::flipRank(from);
    to = SQ::flipRank(to);
  }
  int i = precomputation::encode_from_to[from][to];
  return model.l2->forwardOne(i, tmp[0]);
}

void MoveEvaluator::initialize(const Position& pos) {
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

void MoveEvaluator::update(Color color, PieceType type, Square sq, bool put) {
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
