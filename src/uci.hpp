#include "base.hpp"
#include "position.hpp"
#include "engine.hpp"
#include "mcts/engine.hpp"

enum EventType { kCommandEvent, kSearchResultEvent, kNoEventType };

struct Event {
  EventType type = kNoEventType;
  // TODO:
  //   std::variant<string, SearchResult> should be fine but the compiler on CI didn't accept some code.
  //   So, for now, we simply hold both two exclusive entries.
  string command;
  SearchResult search_result;

  Event(const string& arg) : type{kCommandEvent}, command{arg} {}
  Event(const SearchResult& arg) : type{kSearchResultEvent}, search_result{arg} {}
};

struct UCIOption {
  string id;     // e.g. Hash
  string detail; // e.g. type spin default 16 min 1 max 16384
  std::function<void(std::istream&)> handler;
};

struct UCI {
  std::istream& istr;
  std::ostream& ostr;
  std::ostream& err_ostr;
  vector<UCIOption> options;

  Queue<Event> queue; // single consumer and two producers
  std::future<bool> command_listener_thread_future;

  std::unique_ptr<EngineBase> engine;

  UCI(std::istream&, std::ostream&, std::ostream&);
  int mainLoop();
  void startCommandListenerThread();
  void monitorEvent();
  void cleanup();
  void handleCommand(const string&);
  void handleSearchResult(const SearchResult&);
  void putSearchResult(const SearchResult& result) { queue.put(Event(result)); }

  //
  // I/O helpers
  //

  string readToken(std::istream& istream) {
    string res;
    istream >> res;
    return res;
  }

  template<class ...Ts>
  void print(Ts ...args) { ::_print(ostr, " ", "", args...); ostr << std::endl; }

  template<class ...Ts>
  void printError(Ts ...args) { print("info string ERROR", args...); }

  //
  // UCI commands
  //

  void uci_uci(std::istream&) {
    print("name toy-chess");
    print("author hiro18181");
    for (auto& option : options) { print("option name", option.id, option.detail); }
    print("uciok");
  }

  void uci_debug(std::istream& command) {
    auto value = readToken(command);
    ASSERT(value == "on" || value == "off");
    engine->debug = (value == "on");
  }

  void uci_isready(std::istream&) {
    engine->stop();
    print("readyok");
  }

  void uci_setoption(std::istream& command) {
    // setoption name <id> [value <x>]
    ASSERT(readToken(command) == "name");
    auto id = readToken(command);
    ASSERT(readToken(command) == "value");
    for (auto& option : options) {
      if (option.id == id) { option.handler(command); return; }
    }
    printError("Unsupported option [" + id + "]");
  }

  void uci_register(std::istream&) {
    printError("Unsupported command [register]");
  }

  void uci_ucinewgame(std::istream&) {
    engine->stop();
    engine->reset();
  }

  void uci_position(std::istream&);
  void uci_go(std::istream&);

  void uci_stop(std::istream&) {
    engine->stop();
  }

  void uci_ponderhit(std::istream&) {
    printError("Unsupported command [ponderhit]");
  }

  void toy_debug(std::istream&);
  void toy_perft(std::istream&);
};
