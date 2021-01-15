#include "base.hpp"

namespace Zobrist {
  using Key = uint32_t;

  // Hash seeds
  extern Key side_to_move;
  extern array3<Key, 2, 6, 64> piece_squares;
  extern array2<Key, 2, 2> castling_rights;
  extern array<Key, 64> ep_squares;

  void initializeHashSeeds();

  inline void print(Key key, std::ostream& ostr) {
    auto tmp = ostr.flags();
    ostr << "0x" << std::setfill('0') << std::setw(8) << std::right << std::hex << key;
    ostr.flags(tmp);
  }
}
