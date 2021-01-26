#include "uci.hpp"

UCI::UCI(std::istream& istr, std::ostream& ostr, std::ostream& err_ostr)
  : istr{istr}, ostr{ostr}, err_ostr{err_ostr} {

  // Setup callback for asynchronous Engine::go
  engine.search_result_callback = [this](const SearchResult& search_result) {
    queue.put(Event(search_result));
  };
}

int UCI::mainLoop() {
  startCommandListenerThread();
  monitorEvent();
  cleanup();
  return 0;
}

void UCI::startCommandListenerThread() {
  auto work = [this]() {
    string data;
    while (std::getline(istr, data) && data != "quit") { queue.put(Event(data)); }
    queue.put(Event("quit"));
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
  else if (token == "toy-debug") { toy_debug(command); }
  else if (token == "toy-perft") { toy_perft(command); }

  else {
    printError("Unknown command [" + token + "]");
  }
}

void UCI::handleSearchResult(const SearchResult& result) {
  print(result);
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
  // TODO: To detect 3-folds repetition, we need to keep last six moves
  for (int i = 0; ; i++) {
    // Reset when reaching stack limit
    if (i == Position::kMaxDepth) { engine.position.reset(); i = 0; }

    auto s_move = readToken(command);
    if (s_move.empty()) { break; }

    MoveList move_list;
    engine.position.generateMoves(move_list);
    bool found = 0;
    for (auto move : move_list) {
      if (!engine.position.isLegal(move)) { continue; }
      if (toString(move) == s_move) {
        found = 1;
        engine.position.makeMove(move);
        break;
      }
    }
    ASSERT(found);
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

void UCI::toy_debug(std::istream&) {
  engine.stop();
  engine.print(ostr);
}

void UCI::toy_perft(std::istream& command) {
  engine.stop();
  engine.position.evaluator = nullptr;
  int depth = 1;
  command >> depth;
  auto start = std::chrono::steady_clock::now();
  auto result = engine.position.divide(depth);
  auto finish = std::chrono::steady_clock::now();
  float time = (float)std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count() / 1000;
  int64_t total = 0;
  for (auto [move, cnt] : result) {
    ostr << move << ": " << cnt << "\n";
    total += cnt;
  }
  ostr << "total: " << total << "\n";
  ostr << "time: " << std::fixed << std::setprecision(3) << time << "\n";
  engine.position.evaluator = &engine.evaluator;
}
