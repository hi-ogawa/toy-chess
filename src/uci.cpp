#include "uci.hpp"

void UCI::startCommandListenerThread() {
  auto work = [&]() {
    string data;
    while (std::getline(istr, data) && data != "quit") {
      queue.put(Event({ .type = kCommandEvent, .command = data }));
    }
    queue.put(Event({ .type = kCommandEvent, .command = "quit" }));
  };
  command_listener_thread_future = std::async(work);
}

void UCI::monitorEvent() {
  // Loop until receiving "quit"
  while (true) {
    Event event = queue.get();
    if (event.type == kCommandEvent) {
      if (event.command == "quit") { break; }
      handleCommand(event.command);
    }
    if (event.type == kSearchResultEvent) {
      handleSearchResult(event.search_result);
    }
  }
  engine.stop();
}

void UCI::handleCommand(const string& s_command) {
  std::istringstream command(s_command);
  auto token = readToken(command);
  if (token.empty()) {
    printError("Empty command");
    return;
  }
  // Cf. https://www.shredderchess.com/download/div/uci.zip
  if (token == "uci")        { uci_uci(command); } else
  if (token == "debug")      { uci_debug(command); } else
  if (token == "isready")    { uci_isready(command); } else
  if (token == "setoption")  { uci_setoption(command); } else
  if (token == "register")   { uci_register(command); } else
  if (token == "ucinewgame") { uci_ucinewgame(command); } else
  if (token == "position")   { uci_position(command); } else
  if (token == "go")         { uci_go(command); } else
  if (token == "stop")       { uci_stop(command); } else
  if (token == "ponderhit")  { uci_ponderhit(command); } else {
    printError("Unknown command - " + token);
  }
}

void UCI::handleSearchResult(const SearchResult& result) {
  if (result.type == kSearchResultInfo) {
    print("info " + toString(result));
  }
  if (result.type == kSearchResultBestMove) {
    print("bestmove " + toString(result));
  }
}

void UCI::uci_position(std::istream& command) {
  // position [fen <fenstring> | startpos ] moves <move1> .... <movei>
  auto type = readToken(command);
  if (type != "fen" && type != "startpos") { printError("Invalid position command"); return; }

  string fen;
  if (type == "startpos") {
    fen = kFenInitialPosition;
  } else {
    for (int i = 0; i < 6; i++) {
      if (i > 0) { fen += " "; }
      fen += readToken(command);
    }
  }
  position.initialize(fen);

  auto token = readToken(command);
  if (token.empty()) { return; }
  if (token != "moves") { printError("Invalid position command"); return; }

  // In order to distinguish move types (e.g. castling/enpassant), we need full move generation.
  while (true) {
    auto s_move = readToken(command);
    if (s_move.empty()) { break; }

    position.generateMoves();
    bool found = 0;
    while (auto move = position.move_list->getNext()) {
      if (toString(*move) == s_move) {
        found = 1;
        position.makeMove(*move);
        break;
      }
    }
    assert(found);
  }
  position.initialize(position.toFen()); // Reset internal stack
}

void UCI::uci_go(std::istream& command) {
  string type = readToken(command);
  assert(type == "depth");

  string s_depth = readToken(command);
  assert(!s_depth.empty());
  int depth = std::stoi(s_depth);
  assert(depth >= 1);

  auto work = [&]() { engine.go(position, depth); };
  engine_thread_future = std::async(work);
}
