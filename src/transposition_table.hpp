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

enum NodeType : uint8_t { kPVNode, kCutNode, kAllNode, kNoneNode };

struct TranspositionTable {

  struct alignas(32) Entry {
    uint16_t upper_key = 0; // 2
    Move move = kNoneMove; // 2
    Score score = kScoreNone; // 2
    Score evaluation = kScoreNone; // 2
    NodeType node_type = kNoneNode; // 1
    uint8_t depth = 0; // 1
  };
  static_assert(sizeof(Entry) == 32);

  uint64_t size = 0;
  vector<Entry> data;

  void reset() {
    data.assign(size, {});
  }

  void resize(uint64_t new_size) {
    ASSERT(new_size > 0);
    size = new_size;
    reset();
  }

  void resizeMB(uint64_t mb) {
    resize((mb * (1 << 20)) / sizeof(Entry));
  }

  uint16_t toUpperKey(uint64_t key) { return key >> 48; }

  bool get(uint64_t key, Entry& entry) {
    uint64_t index = key % size;
    entry = data[index];
    return entry.upper_key == toUpperKey(key);
  }

  void put(uint64_t key, const Entry& entry) {
    uint64_t index = key % size;
    data[index] = entry;
    data[index].upper_key = toUpperKey(key);
  }
};

using TTEntry = TranspositionTable::Entry;
