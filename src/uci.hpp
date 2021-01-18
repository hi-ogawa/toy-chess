#include "base.hpp"
#include "position.hpp"
#include "engine.hpp"

enum EventType { kCommandEvent, kSearchResultEvent, kNoEventType };

struct Event {
  EventType type = kNoEventType;
  // TODO:
  //   std::variant<string, SearchResult> should be fine but the compiler on CI didn't accept some code.
  //   So, for now, we simply hold both two exclusive entries.
  string command;
  SearchResult search_result;
};

struct UCI {
  std::istream& istr;
  std::ostream& ostr;
  std::ostream& err_ostr;

  Queue<Event> queue; // single consumer and two producers
  std::future<bool> command_listener_thread_future;

  Engine engine;

  UCI(std::istream&, std::ostream&, std::ostream&);
  int mainLoop();
  void startCommandListenerThread();
  void monitorEvent();
  void cleanup();
  void handleCommand(const string&);
  void handleSearchResult(const SearchResult&);

  //
  // I/O helpers
  //

  string readToken(std::istream& istream) {
    string res;
    istream >> res;
    return res;
  }

  template<class T>
  void print(const T& v) { ostr << v << std::endl; }

  template<class T>
  void printError(const T& v) { err_ostr << "ERROR: " << v << std::endl; }

  //
  // UCI commands
  //

  void uci_uci(std::istream&) {
    print("name toy-chess");
    print("author hiro18181");
    print("option name WeightFile type string default " + Engine::kEmbeddedWeightName);
    print("uciok");
  }

  void uci_debug(std::istream&) {
    printError("Unsupported command [debug]");
  }

  void uci_isready(std::istream&) {
    engine.stop();
    print("readyok");
  }

  void uci_setoption(std::istream& command) {
    // setoption name <id> [value <x>]
    ASSERT(readToken(command) == "name");
    auto id = readToken(command);
    ASSERT(readToken(command) == "value");
    auto value = readToken(command);
    if (id == "WeightFile") { engine.load(value); return; }
    printError("Unsupported command [setoption name " + id + " ...]");
  }

  void uci_register(std::istream&) {
    printError("Unsupported command [register]");
  }

  void uci_ucinewgame(std::istream&) {
    engine.stop();
    engine.reset();
  }

  void uci_position(std::istream&);
  void uci_go(std::istream&);

  void uci_stop(std::istream&) {
    engine.stop();
  }

  void uci_ponderhit(std::istream&) {
    printError("Unsupported command [ponderhit]");
  }

  void toy_debug(std::istream&);
  void toy_perft(std::istream&);
};
