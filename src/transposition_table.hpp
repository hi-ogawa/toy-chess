#pragma once

#include "base.hpp"
#include "move.hpp"
#include "evaluation.hpp"

namespace Zobrist {
  using Key = uint64_t;

  // Hash seeds
  extern Key side_to_move;
  extern array3<Key, 2, 6, 64> piece_squares;
  extern array2<Key, 2, 2> castling_rights;
  extern array<Key, 64> ep_squares;

  void initializeHashSeeds();

  inline void print(Key key, std::ostream& ostr) {
    auto tmp = ostr.flags();
    ostr << "0x" << std::setfill('0') << std::setw(16) << std::right << std::hex << key;
    ostr.flags(tmp);
  }
}

enum NodeType : uint8_t { kPVNode, kCutNode, kAllNode };

struct TranspositionTable {

  struct alignas(16) Entry {
    Zobrist::Key key; // 8 (TODO: Use only upper half bits)
    Move move = kNoneMove; // 2
    Score score = kNoneScore; // 2
    Score evaluation = kNoneScore; // 2
    NodeType node_type; // 1
    uint8_t depth = 0; // 1
    bool hit = 0;
  };
  static_assert(sizeof(Entry) == 32);

  size_t size = 0;
  vector<Entry> data;

  void resize(size_t new_size) {
    size = new_size;
    data.assign(size, {});
  }

  void resizeMB(size_t mb) {
    resize((mb * (1 << 20)) / sizeof(Entry));
  }

  void reset() {
    data.assign(size, {});
  }

  Entry& probe(Zobrist::Key key) {
    auto index = key % size;
    auto& value = data[index];
    value.hit = value.hit && (value.key == key);
    return value;
  }
};

using TTEntry = TranspositionTable::Entry;
