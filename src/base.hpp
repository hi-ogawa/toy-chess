#pragma once

#include "misc.hpp"

//
// Base types
//

using Color = int;
enum { kWhite, kBlack, kBoth };

using PieceType = int;
enum { kPawn, kKnight, kBishop, kRook, kQueen, kKing, kNoPieceType };

using CastlingSide = bool;
enum { kOO, kOOO };

using File = int;
enum { kFileA, kFileB, kFileC, kFileD, kFileE, kFileF, kFileG, kFileH };

using Rank = int;
enum { kRank1, kRank2, kRank3, kRank4, kRank5, kRank6, kRank7, kRank8 };

using Square = int;
enum {
  kA1, kB1, kC1, kD1, kE1, kF1, kG1, kH1,
  kA2, kB2, kC2, kD2, kE2, kF2, kG2, kH2,
  kA3, kB3, kC3, kD3, kE3, kF3, kG3, kH3,
  kA4, kB4, kC4, kD4, kE4, kF4, kG4, kH4,
  kA5, kB5, kC5, kD5, kE5, kF5, kG5, kH5,
  kA6, kB6, kC6, kD6, kE6, kF6, kG6, kH6,
  kA7, kB7, kC7, kD7, kE7, kF7, kG7, kH7,
  kA8, kB8, kC8, kD8, kE8, kF8, kG8, kH8
};

using Direction = int;
enum {
  kDirN = 8, kDirS = -8, kDirE = 1, kDirW = -1,
  kDirNE = kDirN + kDirE,
  kDirNW = kDirN + kDirW,
  kDirSE = kDirS + kDirE,
  kDirSW = kDirS + kDirW,
  kDirNNE = kDirN + kDirNE,
  kDirNNW = kDirN + kDirNW,
  kDirNEE = kDirE + kDirNE,
  kDirNWW = kDirW + kDirNW,
  kDirSSE = kDirS + kDirSE,
  kDirSSW = kDirS + kDirSW,
  kDirSEE = kDirE + kDirSE,
  kDirSWW = kDirW + kDirSW
};

using RayType = int;
enum { kRookRay, kBishopRay, kNoRayType };

using Board = uint64_t;

//
// Square <-> Board convertion
//

inline Board toBB(Square sq) { return 1ULL << sq; }

// Squares from bitboard as an iterator
struct toSQ {
  uint64_t x;
  toSQ(Board x) : x{x} {}
  struct Iterator {
    uint64_t x;
    Square operator*() { return Square(__builtin_ctzll(x)); }
    Iterator& operator++() { x &= x - 1; return *this; }
    bool operator!=(const Iterator& other) { return x != other.x; }
  };
  auto begin() { return Iterator{x}; }
  auto end() { return Iterator{0}; }
  int size() { return __builtin_popcountll(x); }
  Square front() { assert(size() == 1); return *begin(); }
};

//
// Square utility
//

struct SQ {
  static inline Square fromCoords(File file, Rank rank) { return rank * 8 + file; }
  static inline pair<File, Rank> toCoords(Square sq) { return {sq % 8, sq / 8 }; }

  static inline File toFile(Square sq) { return sq % 8; }
  static inline File toRank(Square sq) { return sq / 8; }

  static inline Square flipFile(Square sq) { return sq ^ 7; }
  static inline Square flipRank(Square sq) { return sq ^ (7 << 3); }

  static inline bool isValid(Square sq) { return 0 <= sq && sq < 64; }

  static inline bool isAligned(Square sq1, Square sq2, Square sq3) {
    auto [f1, r1] = toCoords(sq1);
    auto [f2, r2] = toCoords(sq2);
    auto [f3, r3] = toCoords(sq3);
    auto x1 = f2 - f1, y1 = r2 - r1;
    auto x2 = f3 - f1, y2 = r3 - r1;
    return (x1 * y2 - x2 * y1) == 0; // determinant
  }

  static inline pair<Direction, RayType> getRayType(Square from, Square to) {
    auto [f1, r1] = toCoords(from);
    auto [f2, r2] = toCoords(to);
    auto f = f2 - f1, r = r2 - r1;
    auto g = std::gcd(f, r);
    f /= g, r /= g;
    auto dir = fromCoords(f, r);
    auto type = kNoRayType;
    if (f == 0 || r == 0) { type = kRookRay; }
    if (std::abs(f) == 1 && std::abs(r) == 1) { type = kBishopRay; }
    return {dir, type};
  }

  // Printer
  Square square;
  SQ(Square square) : square{square} {}

  void print(std::ostream& ostr = std::cerr) const {
    auto [f, r] = toCoords(square);
    ostr << char(f + 'a') << char(r + '1');
  }

  string toString() const {
    std::ostringstream ostr;
    print(ostr);
    return ostr.str();
  }

  friend std::ostream& operator<<(std::ostream& ostr, const SQ& self) { self.print(ostr); return ostr; }
};


//
// Board utility
//

struct BB {
  static inline Board fromFile(File file) { return 0x0101010101010101ULL << file; }
  static inline Board fromRank(Rank rank) { return 0x00000000000000FFULL << (8 * rank); }

  // Printer
  Board board;
  BB(Board board) : board{board} {}

  void print(std::ostream& ostr = std::cerr, int level = 1) const {
    for (int i = 7; i >= 0; i--) {
      if (level) { ostr << "+---+---+---+---+---+---+---+---+" << "\n"; }
      for (int j = 0; j < 8; j++) {
        char mark = (board & toBB(SQ::fromCoords(j, i))) ? 'X' : (level ? ' ' : '.');
        if (level) { ostr << "| "; }
        ostr << mark;
        if (level) { ostr << " "; }
      }
      if (level) { ostr << "| " << (i + 1); }
      ostr << "\n";
    }
    if (level) {
      ostr << "+---+---+---+---+---+---+---+---+" << "\n";
      ostr << "  a   b   c   d   e   f   g   h  " << "\n";
    }
  }

  string toString(int level = 1) const {
    std::ostringstream ostr;
    print(ostr, level);
    return ostr.str();
  }

  friend std::ostream& operator<<(std::ostream& ostr, const BB& self) { self.print(ostr); return ostr; }
};

//
// More constants
//

const inline array<Rank, 2> kBackrank = {kRank1, kRank8};
const inline array<Board, 2> kBackrankBB = {BB::fromRank(kRank1), BB::fromRank(kRank8)};

const inline array<Direction, 8> kKingDirs = {kDirN, kDirS, kDirE, kDirW, kDirNE, kDirNW, kDirSE, kDirSW};
const inline array<Direction, 8> kKnightDirs = {kDirNNE, kDirNNW, kDirNEE, kDirNWW, kDirSSE, kDirSSW, kDirSEE, kDirSWW};
const inline array<Direction, 4> kBishopDirs = {kDirNE, kDirNW, kDirSE, kDirSW};
const inline array<Direction, 4> kRookDirs = {kDirN, kDirS, kDirE, kDirW};

const inline array<Direction, 2> kPawnPushDirs = {kDirN, kDirS};
const inline array2<Direction, 2, 2> kPawnCaptureDirs = {{ {{kDirNE, kDirNW}}, {{kDirSE, kDirSW}}, }};

// kCastlingMoves[color][side] = {king_from, king_to, rook_from, rook_to}
const inline array3<Square, 2, 2, 4> kCastlingMoves = {{
  {{ {{kE1, kG1, kH1, kF1}}, {{kE1, kC1, kA1, kD1}} }},
  {{ {{kE8, kG8, kH8, kF8}}, {{kE8, kC8, kA8, kD8}} }}
}};

inline std::unordered_map<char, array<int, 2>> kFenPiecesMapping = {
  {'P', {0, 0}}, {'N', {0, 1}}, {'B', {0, 2}}, {'R', {0, 3}}, {'Q', {0, 4}}, {'K', {0, 5}},
  {'p', {1, 0}}, {'n', {1, 1}}, {'b', {1, 2}}, {'r', {1, 3}}, {'q', {1, 4}}, {'k', {1, 5}}};

const inline array<string, 2> kFenPiecesMappingInverse = {"PNBRQK", "pnbrqk"};

const inline char* kFenInitialPosition = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
