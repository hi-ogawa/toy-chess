//
// .pgn -> .slim-pgn
//

//
// NOTE:
//   CCRL's pgn is in CRLF newline format,
//   So we need to convert CRLF to LF before running "slim_pgn", as in
//
//    dos2unix < src/nn/data/CCRL-4040.[1206633].pgn | build/Release/slim_pgn > src/nn/data/CCRL-4040.[1206633].slim-pgn
//

int main() {
  std::ios_base::sync_with_stdio(0);
  std::cin.tie(0);

  std::string line;
  std::string moves;
  int64_t counter = 0;
  while (std::getline(std::cin, line)) {
    if (line.empty()) {
      if (!moves.empty()) {
        moves.erase(moves.end() - 1); // Remove final space
        std::cout << moves << "\n";
        moves.clear();
        if (++counter % 100000 == 0) { std::cerr << ":: counter = " << counter << std::endl; }
      }
      continue;
    }
    if (line[0] == '[') { continue; }
    moves += line;
    moves += " ";
  }
  return 0;
}
