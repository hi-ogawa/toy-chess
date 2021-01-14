#include "uci.hpp"

UCI::UCI(std::istream& istr, std::ostream& ostr, std::ostream& err_ostr)
  : istr{istr}, ostr{ostr}, err_ostr{err_ostr} {

  // Setup callback for asynchronous Engine::go
  engine.search_result_callback = [&](const SearchResult& search_result) {
    queue.put(Event({ .type = kSearchResultEvent, .search_result = search_result }));
  };
}

int UCI::mainLoop() {
  startCommandListenerThread();
  monitorEvent();
  cleanup();
  return 0;
}

void UCI::startCommandListenerThread() {
  auto work = [&]() {
    string data;
    while (std::getline(istr, data) && data != "quit") {
      queue.put(Event({ .type = kCommandEvent, .command = data }));
    }
    queue.put(Event({ .type = kCommandEvent, .command = "quit" }));
    return true;
  };
  command_listener_thread_future = std::async(work);
}

void UCI::monitorEvent() {
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
}

void UCI::cleanup() {
  engine.stop();
  ASSERT(command_listener_thread_future.valid());
  ASSERT(command_listener_thread_future.wait_for(std::chrono::milliseconds(100)) == std::future_status::ready);
  ASSERT(command_listener_thread_future.get());
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
  if (token == "ponderhit")  { uci_ponderhit(command); }

  // Custom commands
  else if (token == "toy-debug") { engine.stop(); engine.print(err_ostr); }

  else {
    printError("Unknown command [" + token + "]");
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
  engine.stop();

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
  engine.position.initialize(fen);

  auto token = readToken(command);
  if (token.empty()) { return; }
  if (token != "moves") { printError("Invalid position command"); return; }

  // In order to distinguish move types (e.g. castling/enpassant), we need full move generation.
  while (true) {
    auto s_move = readToken(command);
    if (s_move.empty()) { break; }

    engine.position.generateMoves();
    bool found = 0;
    while (auto move = engine.position.move_list->getNext()) {
      if (toString(*move) == s_move) {
        found = 1;
        engine.position.makeMove(*move);
        break;
      }
    }
    assert(found);
  }
  engine.position.reset();
}

void UCI::uci_go(std::istream& command) {
  engine.stop();

  string token;
  auto& params = engine.go_parameters;
  params = {};
  while (command >> token) {
    if (token == "searchmoves") { ASSERT(0); }
    if (token == "ponder") { ASSERT(0); }
    if (token == "wtime") { command >> params.time[kWhite]; }
    if (token == "btime") { command >> params.time[kBlack]; }
    if (token == "winc") { command >> params.inc[kBlack]; }
    if (token == "binc") { command >> params.inc[kBlack]; }
    if (token == "movestogo") { command >> params.movestogo; }
    if (token == "depth") { command >> params.depth; ASSERT(params.depth > 0); }
    if (token == "nodes") { ASSERT(0); }
    if (token == "mate") { ASSERT(0); }
    if (token == "movetime") { command >> params.movetime; }
    if (token == "infinite") { params.depth = Position::kMaxDepth; }
  }

  engine.go(/* blocking */ false);
}
