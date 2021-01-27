#pragma once

#include "../misc.hpp"
#include "../position_fwd.hpp"
#include "../precomputation.hpp"
#include "utils.hpp"
#include "evaluator.hpp"

namespace nn {

inline constexpr int WIDTH_OUT = precomputation::kFromToEncodingSize;

struct MoveModel {

  // NOTE: Allocate on heap since weights are too large for stack
  std::unique_ptr<InputLayer<WIDTH1, WIDTH2   >> l1;
  std::unique_ptr<Linear<2 * WIDTH2, WIDTH_OUT>> l2;

  MoveModel() {
    l1.reset(new decltype(l1)::element_type);
    l2.reset(new decltype(l2)::element_type);
  }

  void load(const string& filename) {
    std::ifstream istr(filename, std::ios_base::in | std::ios_base::binary);
    ASSERT(istr);
    load(istr);
    istr.peek();
    ASSERT(istr.eof());
  }

  void load(std::istream& istr) {
    l1->load(istr);
    l2->load(istr);
  }

  void loadEmbeddedWeight() {
    CharStreambuf buf(const_cast<char*>(kEmbeddedMoveWeight), kEmbeddedMoveWeightSize);
    std::istream istr(&buf);
    load(istr);
    istr.peek();
    ASSERT(istr.eof());
  }
};

struct MoveEvaluator {
  MoveModel model;
  array<Square, 2> kings;

  alignas(kMaxFloatVectorSize) float accumulator[2][WIDTH2] = {};

  void load(const string& filename) { model.load(filename); }
  void loadEmbeddedWeight() { model.loadEmbeddedWeight(); }

  float evaluate(Color, Square, Square);
  void initialize(const Position&);
  void update(Color, PieceType, Square, bool);
  void putPiece(Color color, PieceType type, Square to) { update(color, type, to, true); }
  void removePiece(Color color, PieceType type, Square from) { update(color, type, from, false); }
};

}; // namespace nn
