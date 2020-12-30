#include "precomputation.hpp"
#include <config.hpp>

namespace precomputation {

  // max{ |dx|, |dy| } (i.e. sup norm)
  array2<uint8_t, 64, 64> distance_table = {};

  // Squares (strictly) in-between two squares
  array2<Board, 64, 64> in_between_table = {};

  // Non-sliding piece attack table
  array<Board, 64> king_attack_table = {};
  array<Board, 64> knight_attack_table = {};
  array2<Board, 2, 64> pawn_attack_table = {}; // Capture for white/black

  // Magic sliding piece attack table
  array<Magic, 64> rook_magic_table = {};
  array<Magic, 64> bishop_magic_table = {};
  vector<Board> rook_attack_table = {};
  vector<Board> bishop_attack_table = {};
  bool is_magic_ready = 0;

  //
  // Table generation
  //

  void initializeTables() {
    generateDistanceTable();
    generateInBetweenTable();
    generateNonSlidingAttackTables();
    initializeMagicTables();
  }

  void generateDistanceTable() {
    for (Square from = 0; from < 64; from++) {
      for (Square to = 0; to < 64; to++) {
        auto [x1, y1] = SQ::toCoords(from);
        auto [x2, y2] = SQ::toCoords(to);
        distance_table[from][to] = std::max(std::abs(x1 - x2), std::abs(y1 - y2));
      }
    }
  }

  void generateInBetweenTable() {
    for (Square from = 0; from < 64; from++) {
      for (auto step : kKingDirs) {
        Board b(0);
        Square sq = from;
        while (true) {
          Square to(sq + step);
          if (!SQ::isValid(to) || distance_table[sq][to] != 1) { break; }
          in_between_table[from][to] = b;
          b = b | toBB(to);
          sq = to;
        }
      }
    }
  }

  void generateNonSlidingAttackTables() {
    for (Square from = 0; from < 64; from++) {
      // King
      for (auto dir : kKingDirs) {
        Square to(from + dir);
        if (SQ::isValid(to) && distance_table[from][to] == 1) {
          king_attack_table[from] |= toBB(to);
        }
      }

      // Knight
      for (auto dir : kKnightDirs) {
        Square to(from + dir);
        if (SQ::isValid(to) && distance_table[from][to] == 2) {
          knight_attack_table[from] |= toBB(to);
        }
      }

      // Pawn
      for (Color color = 0; color < 2; color++) {
        for (auto dir : kPawnCaptureDirs[color]) {
          Square to(from + dir);
          if (SQ::isValid(to) && distance_table[from][to] == 1) {
            pawn_attack_table[color][from] |= toBB(to);
          }
        }
      }
    }
  }

  template<class F>
  void generateMagicTable(array<Magic, 64>& magic_table, F getAttack) {
    const Board fileAH = BB::fromFile(kFileA) | BB::fromFile(kFileH);
    const Board rank18 = BB::fromRank(kRank1) | BB::fromRank(kRank8);

    for (Square sq = 0; sq < 64; sq++) {
      // Sliding attack in empty board
      Board mask = getAttack(sq, Board(0));

      // Boundary occupancy is irrelavant
      auto [file, rank] = SQ::toCoords(sq);
      Board edge = (fileAH & ~BB::fromFile(file)) | (rank18 & ~BB::fromRank(rank));
      mask &= ~edge;

      int mask_popcnt = toSQ(mask).size();
      assert(5 <= mask_popcnt && mask_popcnt <= 12);

      // Enumerate all relavant occupancy configuration
      vector<array<uint64_t, 2>> mapping;
      for (Board occ = 0; ;) {
        mapping.push_back({occ, getAttack(sq, occ)});
        occ = (occ - mask) & mask;
        if (occ == 0) { break; }
      }
      assert(mapping.size() == (1 << mask_popcnt));

      // NOTE: For debug build, allow one more bit to make it easier to find magic (otherwise it's too slow)
      int n = mask_popcnt + (kBuildType == "Debug");

      Magic& m = magic_table[sq];
      m.mask = mask;
      m.shift = 64 - n;

      const int num_trial = 1 << 20;
      Rng rng;

      const uint64_t not_used = -1;
      vector<uint64_t> table(1 << n);

      bool found = 0;
      for (int i = 0; i < num_trial; i++) {
        // Look for "promising magic"
        m.magic = 0;
        while (__builtin_popcountll(m.index(mask)) < n - 2) {
          // Generate sparse bits
          m.magic = rng.next64() & rng.next64() & rng.next64();
        }

        // Check if this magic produces conflict
        table.assign(1 << n, not_used);

        bool conflict = 0;
        for (auto [k, v] : mapping) {
          uint64_t h = m.index(k);
          if (table[h] != not_used && table[h] != v) { conflict = 1; break; }
          table[h] = v;
        }
        if (!conflict) { found = 1; break; }
      }
      assert(found);
    }
  }

  template<class F>
  void generateMagicAttackTable(array<Magic, 64>& magic_table, vector<Board>& attack_table, F getAttack) {
    // First allocate for all squares to avoid vector reallocation
    int total_size = 0;
    for (int sq = 0; sq < 64; sq++) {
      auto& m = magic_table[sq];
      total_size += (1 << (64 - m.shift));
    }
    attack_table.resize(total_size);

    // Set pointer to each square's offset
    for (int sq = 0, offset = 0; sq < 64; sq++) {
      auto& m = magic_table[sq];
      m.table = &attack_table[offset];
      for (Board occ = 0; ; ) {
        m.table[m.index(occ)] = getAttack(sq, occ);
        occ = (occ - m.mask) & m.mask;
        if (occ == 0) { break; }
      }
      offset += (1 << (64 - m.shift));
    }
  }

  void initializeMagicTables() {
    auto rookAttack = [&](Square sq, Board occ) -> Board { return getRays(sq, kRookDirs, occ); };
    auto bishopAttack = [&](Square sq, Board occ) -> Board { return getRays(sq, kBishopDirs, occ); };
    if (!is_magic_ready) {
      generateMagicTable(rook_magic_table, rookAttack);
      generateMagicTable(bishop_magic_table, bishopAttack);
      is_magic_ready = 1;
    }
    generateMagicAttackTable(rook_magic_table, rook_attack_table, rookAttack);
    generateMagicAttackTable(bishop_magic_table, bishop_attack_table, bishopAttack);
  }

  //
  // Attack board based on tables
  //

  Board getRay(Square from, Direction dir, Board occ) {
    Board ray(0);
    Square sq = from;
    while (true) {
      auto to = sq + dir;
      if (!SQ::isValid(to) || distance_table[sq][to] != 1) { break; }
      ray |= toBB(to);
      if (ray & occ) { break; }
      sq = to;
    }
    return ray;
  }

  Board getMagicAttack(Square from, Board occ, const array<Magic, 64>& magic_table) {
    Magic m = magic_table[from];
    return m.table[m.index(occ)];
  }

  Board getBishopAttack(Square from, Board occ) { return getMagicAttack(from, occ, bishop_magic_table); }
  Board getRookAttack(Square from, Board occ)   { return getMagicAttack(from, occ, rook_magic_table); }
  Board getQueenAttack(Square from, Board occ)  { return getRookAttack(from, occ) | getBishopAttack(from, occ); }

} // namespace precomputation
