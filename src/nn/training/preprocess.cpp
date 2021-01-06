//
// .binpack -> .halfkp
//

#include "../../misc.hpp"


// Use Tomasz Sobczyk's binpack routines
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wall"
#include "../../../thirdparty/Stockfish/src/extra/nnue_data_binpack_format.h"
#pragma clang diagnostic pop


void makeIndices(const chess::Position& pos, uint16_t* x_w, uint16_t* x_b) {
  using namespace chess;

  auto flip = [](int sq) -> int { return sq ^ (7 << 3); };

  int k_w = int(pos.kingSquare(Color::White));
  int k_b = int(pos.kingSquare(Color::Black));

  int cnt = 0;
  for (int sq = 0; sq < 64; sq++) {
    Piece piece = pos.pieceAt(Square(sq));
    Color color = piece.color();
    PieceType type = piece.type();
    if (type == PieceType::None) { continue; }
    if (type == PieceType::King) { continue; }
    cnt++;
    ASSERT(cnt <= 32);

    int t = int(type);
    int t_w = t + 5 * (color == Color::Black);
    int t_b = t + 5 * (color == Color::White);
    *(x_w++) = (t_w * 64 +      sq ) * 64 +      k_w ;
    *(x_b++) = (t_b * 64 + flip(sq)) * 64 + flip(k_b);
  }
}

void preprocess(const string& infile, const string& outfile, bool shuffle, int buffer_size, int eval_limit) {

  binpack::CompressedTrainingDataEntryReader binpack_reader(infile);

  std::ofstream ostr(outfile, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
  assert(ostr);

  // Each entry is 128bytes (4 times worse than packed sfen format)
  using Entry = array<uint16_t, 64>;
  const uint16_t kEmbeddingPad = 10 * 64 * 64;

  vector<Entry> entries;
  entries.reserve(buffer_size);

  const int kShuffleSeed = 0x12345678;
  auto rng = std::mt19937(kShuffleSeed);

  int64_t counter = 0;
  int64_t skip_counter = 0;
  for (; binpack_reader.hasNext(); counter++) {
    if (counter % 1000000 == 0) { std::cerr << ":: counter = " << counter << std::endl; }
    binpack::TrainingDataEntry e = binpack_reader.next();
    if (std::abs(e.score) > eval_limit) { skip_counter++; continue; }

    Entry& entry = entries.emplace_back();
    std::fill(entry.begin(), entry.end(), kEmbeddingPad);
    uint16_t* x_w = &entry[0];
    uint16_t* x_b = &entry[32];

    // NOTE: gensfen's score is "side-to-move" perspective, but we want evaluation to be "white" perspective.
    if (e.pos.sideToMove() == chess::Color::Black) { std::swap(x_w, x_b); }

    makeIndices(e.pos, x_w, x_b);

    // Sneak score into the last unused index
    entry[63] = e.score;

    // Shuffle and write data
    if (!binpack_reader.hasNext() || (int)entries.size() == buffer_size) {
      if (shuffle) {
        std::shuffle(entries.begin(), entries.end(), rng);
      }
      ostr.write(reinterpret_cast<char*>(&entries[0]), sizeof(entries[0]) * entries.size());
      ostr.flush();
      entries.clear();
    }
  }

  std::cerr << ":: skip_counter = " << skip_counter << std::endl;
}

int main(int argc, const char* argv[]) {
  Cli cli{argc, argv};
  auto infile = cli.getArg<string>("--infile");
  auto outfile = cli.getArg<string>("--outfile");
  bool shuffle = cli.getArg<int>("--shuffle").value_or(1);
  int buffer_size = cli.getArg<int>("--buffer-size").value_or(500000);
  int eval_limit = cli.getArg<int>("--eval-limit").value_or(3000); // NOTE: gensfen's "eval_limit" doesn't perfectly limit, so here we filter again.
  if (!infile) {
    std::cerr << cli.help() << std::endl;
    return 1;
  }
  if (!std::fstream(*infile).good()) {
    std::cerr << ":: Invalid infile" << std::endl;
    return 1;
  }
  if (!outfile) {
    outfile = infile->substr(0, infile->size() - string("binpack").size()) + "halfkp";
  }
  preprocess(*infile, *outfile, shuffle, buffer_size, eval_limit);
  return 0;
}
