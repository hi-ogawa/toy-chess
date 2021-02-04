#pragma once

#include "../misc.hpp"
#include "../base.hpp"
#include "../position_fwd.hpp"
#include "../nn/utils.hpp"

namespace zero {

// Constants from main.py
inline constexpr int WIDTH1 = 10 * 64 * 64; // = 40960
inline constexpr int WIDTH2 = 256;
inline constexpr int WIDTH_POLICY = 1792;
inline constexpr int POLICY_TOP_K = 16;

struct Model {
  std::unique_ptr<nn::InputLayer<WIDTH1, WIDTH2      >> embedding;
  std::unique_ptr<nn::Linear<2 * WIDTH2, WIDTH_POLICY>> fc_policy;
  std::unique_ptr<nn::Linear<2 * WIDTH2, 1           >> fc_value;

  Model() {
    embedding.reset(new decltype(embedding)::element_type);
    fc_policy.reset(new decltype(fc_policy)::element_type);
    fc_value.reset(new decltype(fc_value)::element_type);
  }

  void load(const string& filename) {
    std::ifstream istr(filename, std::ios_base::in | std::ios_base::binary);
    ASSERT(istr);
    load(istr);
    istr.peek();
    ASSERT(istr.eof());
  }

  void load(std::istream& istr) {
    embedding->load(istr);
    fc_policy->load(istr);
    fc_value->load(istr);
  }
};

struct Evaluator {
  Model model;
  array<Square, 2> kings;
  alignas(nn::kMaxFloatVectorSize) float accumulator[2][WIDTH2] = {};

  void load(const string& filename) { model.load(filename); }

  float computeWDL(Color color);
  float computePolicy(Color color, Square from, Square to);

  void initialize(Position& position);
  void putPiece(Color color, PieceType type, Square to) { update(color, type, to, true); }
  void removePiece(Color color, PieceType type, Square from) { update(color, type, from, false); }
  void update(Color color, PieceType type, Square sq, bool put);
};

} // namespace zero
