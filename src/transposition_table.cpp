#include "transposition_table.hpp"

namespace {
  // Auto initialize on startup
  struct RunOnStartup {
    RunOnStartup() {
      Zobrist::initializeHashSeeds();
    }
  } run_on_startup;
};


namespace Zobrist {

// Hash seeds
Key side_to_move;
array3<Key, 2, 6, 64> piece_squares;
array2<Key, 2, 2> castling_rights;
array<Key, 64> ep_squares;

void initializeHashSeeds() {
  Rng rng;

  side_to_move = rng.next64();

  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 6; j++) {
      for (int k = 0; k < 64; k++) {
        piece_squares[i][j][k] = rng.next64();
      }
    }
  }

  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 2; j++) {
      castling_rights[i][j] = rng.next64();
    }
  }

  for (int i = 0; i < 64; i++) {
    ep_squares[i] = rng.next64();
  }
}

} // namespace Zobrist
