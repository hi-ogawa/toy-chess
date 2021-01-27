//
// .slim-pgn -> .halkp-move
//
// E.g.
//   build/Release/nn_preprocess_move --infile "src/nn/data/CCRL-4040.[1206633].slim-pgn"
//

#include "../../misc.hpp"
#include "../../position.hpp"
#include "../../precomputation.hpp"

void preprocess(const string& infile, const string& outfile, int minply, int maxply, bool shuffle, int buffer_size) {
  std::ifstream istr(infile);

  std::ofstream ostr(outfile, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
  ASSERT(ostr);

  // 128bytes per entry
  using Entry = array<uint16_t, 64>;
  const uint16_t kEmbeddingPad = 10 * 64 * 64;

  vector<Entry> entries;
  entries.reserve(buffer_size);

  Position pos;

  auto write_entry = [&](const Move& move) {
    Entry& entry = entries.emplace_back();
    std::fill(entry.begin(), entry.end(), kEmbeddingPad);

    uint16_t* x_w = &entry[0];  // [0:31]
    uint16_t* x_b = &entry[31]; // [31:62]
    uint16_t* y = &entry[62];  // [62:63]

    // Position and move are white perspective
    if (pos.side_to_move == kBlack) { std::swap(x_w, x_b); }
    pos.writeHalfKP(x_w, x_b);

    // Encode (from, to)
    Square from = move.from();
    Square to = move.to();
    if (pos.side_to_move == kBlack) {
      from = SQ::flipRank(from);
      to = SQ::flipRank(to);
    }
    *y = precomputation::encode_from_to[from][to];
  };

  const int kShuffleSeed = 0x12345678;
  auto rng = std::mt19937(kShuffleSeed);

  string line, token;
  for (int64_t counter = 0; ; counter++) {
    if (counter % 25000 == 0) { std::cerr << ":: counter = " << counter << std::endl; }

    std::getline(istr, line);
    istr.peek();

    if (istr.eof() || (int)entries.size() >= buffer_size) {
      if (shuffle) { std::shuffle(entries.begin(), entries.end(), rng); }
      ostr.write(reinterpret_cast<char*>(&entries[0]), sizeof(entries[0]) * entries.size());
      entries.clear();
    }
    if (istr.eof()) { break; }

    std::istringstream sstr(line);
    pos.initialize(kFenInitialPosition);
    for (int i = 0; pos.game_ply < maxply; i += 2) {
      // Reset internal stack
      if (i >= 200) { pos.reset(); i = 0; }

      // Move counter or result
      sstr >> token;
      if (token == "1-0" || token == "0-1" || token == "1/2-1/2") { break; }

      // White move
      sstr >> token;
      Move w_move = pos.parsePgnMove(token);
      ASSERT_HOT(pos.isLegal(w_move));
      if (pos.game_ply >= minply) { write_entry(w_move); }
      pos.makeMove(w_move);

      // Black move or result
      sstr >> token;
      if (token == "1-0" || token == "0-1" || token == "1/2-1/2") { break; }
      Move b_move = pos.parsePgnMove(token);
      ASSERT_HOT(pos.isLegal(b_move));
      if (pos.game_ply >= minply) { write_entry(b_move); }
      pos.makeMove(b_move);
    }
  }
}

int main(int argc, const char* argv[]) {
  Cli cli{argc, argv};
  auto infile = cli.getArg<string>("--infile");
  auto outfile = cli.getArg<string>("--outfile");
  bool shuffle = cli.getArg<int>("--shuffle").value_or(1);
  int buffer_size = cli.getArg<int>("--buffer-size").value_or(500000);
  int minply = cli.getArg<int>("--buffer-size").value_or(10);
  int maxply = cli.getArg<int>("--buffer-size").value_or(300);
  if (!infile) {
    std::cerr << cli.help() << std::endl;
    return 1;
  }
  if (!std::fstream(*infile).good()) {
    std::cerr << ":: Invalid infile" << std::endl;
    return 1;
  }
  if (!outfile) {
    outfile = infile->substr(0, infile->size() - string("slim-pgn").size()) + "halfkp-move";
  }
  preprocess(*infile, *outfile, minply, maxply, shuffle, buffer_size);
  return 0;
}
