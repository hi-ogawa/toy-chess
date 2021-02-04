#include "evaluator.hpp"
#include "../position.hpp"

namespace zero {

float Evaluator::computeWDL(Color color) {
  alignas(nn::kMaxFloatVectorSize) float tmp[2 * WIDTH2];
  float result = 0;
  nn::relu<2 * WIDTH2>(accumulator[0], tmp);
  model.fc_value->forward(tmp, &result);
  return ((color == kWhite) ? +1 : -1) * result;
}

float Evaluator::computePolicy(Color color, Square from, Square to) {
  alignas(nn::kMaxFloatVectorSize) float tmp[2][WIDTH2];
  // TODO: Should cache relu-ed accumulator
  if (color == kWhite) {
    nn::relu<WIDTH2>(accumulator[0], tmp[0]);
    nn::relu<WIDTH2>(accumulator[1], tmp[1]);
  } else {
    nn::relu<WIDTH2>(accumulator[0], tmp[1]);
    nn::relu<WIDTH2>(accumulator[1], tmp[0]);
    from = SQ::flipRank(from);
    to = SQ::flipRank(to);
  }
  int index = precomputation::encode_from_to[from][to];
  float result = model.fc_policy->forwardOne(index, tmp[0]);
  return result;
}

void Evaluator::initialize(Position& position) {
  nn::copy<WIDTH2>(model.embedding->bias, accumulator[0]);
  nn::copy<WIDTH2>(model.embedding->bias, accumulator[1]);

  kings[0] = position.kingSQ(kWhite);
  kings[1] = SQ::flipRank(position.kingSQ(kBlack));

  for (Color color = 0; color < 2; color++) {
    for (PieceType type = 0; type < 5; type++) {
      for (auto sq : toSQ(position.pieces[color][type])) {
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
    nn::add<WIDTH2>(accumulator[0], model.embedding->weight[index_w], accumulator[0]);
    nn::add<WIDTH2>(accumulator[1], model.embedding->weight[index_b], accumulator[1]);
  } else {
    nn::sub<WIDTH2>(accumulator[0], model.embedding->weight[index_w], accumulator[0]);
    nn::sub<WIDTH2>(accumulator[1], model.embedding->weight[index_b], accumulator[1]);
  }
}

} // namespace zero
