#pragma once

#include "base.hpp"

namespace precomputation {

  //
  // Tables
  //

  // max{ |dx|, |dy| } (i.e. sup norm)
  extern array2<uint8_t, 64, 64> distance_table;

  // Squares (strictly) in-between two squares
  extern array2<Board, 64, 64> in_between_table;

  // Non-sliding piece attack table
  extern array<Board, 64> king_attack_table;
  extern array<Board, 64> knight_attack_table;
  extern array2<Board, 2, 64> pawn_attack_table; // Capture for white/black

  // Magic sliding piece attack table
  struct Magic {
    Board* table;
    uint64_t magic, mask;
    int shift;
    uint64_t index(uint64_t occ) { return ((occ & mask) * magic) >> shift; };
  };
  extern array<Magic, 64> rook_magic_table;
  extern array<Magic, 64> bishop_magic_table;
  extern vector<Board> rook_attack_table;
  extern vector<Board> bishop_attack_table;
  extern bool is_magic_ready;

  // std::getenv("NO_PRECOMPUTATION_INIT")
  void initializeTables(); // TODO: automatically run on startup (e.g. static struct trick)
  void generateDistanceTable();
  void generateInBetweenTable();
  void generateNonSlidingAttackTables();
  void initializeMagicTables();

  //
  // Attack squares
  //

  Board getRay(Square, Direction, Board);

  template<class Iterable>
  inline Board getRays(Square from, Iterable dirs, Board occ) {
    Board b(0);
    for (auto dir : dirs) { b |= getRay(from, dir, occ); }
    return b;
  }

  Board getBishopAttack(Square, Board);
  Board getRookAttack(Square, Board);
  Board getQueenAttack(Square, Board);

} // namespace precomputation
