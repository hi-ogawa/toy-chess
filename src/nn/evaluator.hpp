#pragma once

#include "../misc.hpp"
#include "../evaluation.hpp" // Score
#include "../position_fwd.hpp"
#include "utils.hpp"
#include "embedded_weight.hpp"

namespace nn {

// Constants from main.py
inline constexpr int WIDTH1 = 10 * 64 * 64;
inline constexpr int WIDTH2 = 128;
inline constexpr int WIDTH3 = 32;
inline constexpr int WIDTH4 = 32;

struct MyModel {

  // NOTE: Allocate on heap since weights are too large for stack
  std::unique_ptr<InputLayer<WIDTH1, WIDTH2>> l1;
  std::unique_ptr<Linear<2 * WIDTH2, WIDTH3>> l2;
  std::unique_ptr<Linear<    WIDTH3, WIDTH4>> l3;
  std::unique_ptr<Linear<    WIDTH4,      1>> l4;

  MyModel() {
    l1.reset(new decltype(l1)::element_type);
    l2.reset(new decltype(l2)::element_type);
    l3.reset(new decltype(l3)::element_type);
    l4.reset(new decltype(l4)::element_type);
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
    l3->load(istr);
    l4->load(istr);
  }

  void loadEmbeddedWeight() {
    // NOTE: Make istream from char array (Cf. https://stackoverflow.com/questions/7781898/get-an-istream-from-a-char)
    struct CharStreambuf : std::streambuf {
      CharStreambuf(char* ptr, int size) { setg(ptr, ptr, ptr + size); }
    };
    CharStreambuf buf(const_cast<char*>(kEmbeddedWeight), kEmbeddedWeightSize);

    std::istream istr(&buf);
    load(istr);

    istr.peek();
    ASSERT(istr.eof());
  }
};

struct Evaluator {
  MyModel model;

  alignas(kFloatVectorSize) Float accumulator[2][WIDTH2] = {};
  alignas(kFloatVectorSize) Float tmp2[2 * WIDTH2] = {};
  alignas(kFloatVectorSize) Float tmp3[WIDTH3] = {};
  alignas(kFloatVectorSize) Float tmp4[WIDTH4] = {};
  Float tmp5 = 0;

  array<Square, 2> kings;

  void load(const string& filename) { model.load(filename); }
  void loadEmbeddedWeight() { model.loadEmbeddedWeight(); }

  Score evaluate();

  void initialize(const Position&);

  // Incremental update
  void update(Color, PieceType, Square, bool);
  void putPiece(Color color, PieceType type, Square to) { update(color, type, to, true); }
  void removePiece(Color color, PieceType type, Square from) { update(color, type, from, false); }
};

}; // namespace nn
