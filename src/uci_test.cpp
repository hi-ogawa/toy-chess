#include "uci.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
using Catch::Matchers::Contains;

// Piped in/out stream pair (cf. https://stackoverflow.com/questions/2746168/how-to-construct-a-c-fstream-from-a-posix-file-descriptor)
#include <ext/stdio_filebuf.h>
#include <unistd.h>

struct PipeStream {
  int fds[2];
  __gnu_cxx::stdio_filebuf<char> bufs[2];
  std::unique_ptr<std::istream> reader; // Workaround for deleted default constructor
  std::unique_ptr<std::ostream> writer;

  PipeStream() {
    ASSERT(pipe(fds) != -1);
    bufs[0] = {fds[0], std::ios::in};
    bufs[1] = {fds[1], std::ios::out};
    reader.reset(new std::istream{&bufs[0]});
    writer.reset(new std::ostream{&bufs[1]});
  }

  ~PipeStream() {
    close(fds[0]);
    close(fds[1]);
  }

  std::istream& getReader() { return *reader; }
  std::ostream& getWriter() { return *writer; }
};


// UCI input/output testing
struct UCITester {
  PipeStream istr, ostr, err_ostr;
  std::future<int> uci_future;

  UCITester() {
    uci_future = std::async([&]() {
      UCI uci(istr.getReader(), ostr.getWriter(), err_ostr.getWriter());
      return uci.mainLoop();
    });
  }

  template<class Duration>
  std::future_status waitFor(Duration duration) {
    return uci_future.wait_for(duration);
  }

  void putLine(const string& line) {
    istr.getWriter() << line << std::endl;
  }

  string getLine() {
    string line;
    std::getline(ostr.getReader(), line);
    return line;
  }

  template<class PredicateT>
  bool getLineUntil(PredicateT predicate) {
    string line;
    while (std::getline(ostr.getReader(), line)) {
      if (predicate(line)) { return true; }
    }
    return false;
  }

  bool putAndCheck(const string& input, const vector<string>& outputs) {
    int ok = 1;
    putLine(input);
    for (auto output : outputs) { ok &= (getLine() == output); }
    return ok;
  }
};


TEST_CASE("UCI") {
  UCITester tester;

  CHECK(tester.putAndCheck("uci", {{
    "name toy-chess",
    "author hiro18181",
    "option name Hash type spin default 128 min 1 max 16384",
    "option name WeightFile type string default __EMBEDDED_WEIGHT__",
    "option name Debug type check default false",
    "option name UseMCTS type check default false",
    "option name MCTS_cpuct type string default 1",
    "uciok",
  }}) == 1);

  CHECK(tester.putAndCheck("isready", {{
    "readyok",
  }}) == 1);

  CHECK(tester.putAndCheck("position fen 8/2k5/7R/6R1/8/4K3/8/8 w - - 0 1", {}) == 1);

  tester.putLine("go depth 4");
  CHECK(tester.getLineUntil([&](auto line) { return line == "bestmove g5g7"; }) == true);

  tester.putLine("quit");
  CHECK(tester.waitFor(std::chrono::milliseconds(100)) == std::future_status::ready);
}
