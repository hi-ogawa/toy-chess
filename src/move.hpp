#pragma once

#include "base.hpp"

enum MoveType { kNoMoveType, kNormal, kCastling, kEnpassant, kPromotion };

struct Move {
  // [type]           [to square]  [from square]
  // XXXX             YYYYYY       ZZZZZZ
  // 0000 None
  // 0001 Normal
  // 0010 Castling
  // 0011 Enpassant
  // 0100 Promotion knight
  // 0101 Promotion bishop
  // 0110 Promotion rook
  // 0111 Promotion queen
  uint16_t data = 0;

  Move() {}

  Move(Square from, Square to, MoveType type = kNormal, PieceType promotion_type = 0) {
    uint16_t lo = from;
    uint16_t mi = to;
    uint16_t hi = type;
    if (type == kPromotion) { hi = promotion_type + 3; }
    data = (hi << 12) | (mi << 6) | lo;
  }

  Square from() const { return Square(data & 0b111111U); }

  Square to() const { return Square((data >> 6) & 0b111111U); }

  MoveType type() const {
    auto hi = (data >> 12);
    return (hi & 0b100) ? kPromotion : MoveType(hi);
  }

  PieceType promotionType() const {
    return PieceType((data >> 12) - 3);
  }

  CastlingSide castlingSide() const {
    return ((to() - from()) > 0) ? kOO : kOOO;
  }

  Square capturedPawnSquare() const {
    return SQ::fromCoords(to() % 8, from() / 8);
  }

  void print(std::ostream& ostr = std::cerr) const {
    if (type() == kNoMoveType) { ostr << "NONE"; return; }
    ostr << SQ(from()) << SQ(to());
    if (type() == kPromotion) { ostr << kFenPiecesMappingInverse[1][promotionType()]; }
  }

  friend std::ostream& operator<<(std::ostream& ostr, const Move& self) { self.print(ostr); return ostr; }
};
