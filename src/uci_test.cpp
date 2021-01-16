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

  bool putAndCheck(const string& input, const vector<string>& outputs) {
    int ok = 1;
    putLine(input);
    for (auto output : outputs) { ok &= (getLine() == output); }
    return ok;
  }

  template<class F>
  void putAndCheck_v2(const string& input, const vector<string>& outputs, F check) {
    putLine(input);
    for (auto output : outputs) { check(getLine(), output); }
  }
};


TEST_CASE("UCI") {
  UCITester tester;

  CHECK(tester.putAndCheck("uci", {{
    "name toy-chess",
    "author hiro18181",
    "uciok",
  }}) == 1);

  CHECK(tester.putAndCheck("isready", {{
    "readyok",
  }}) == 1);

  CHECK(tester.putAndCheck("position fen 8/2k5/7R/6R1/8/4K3/8/8 w - - 0 1", {}) == 1);

  tester.putLine("go depth 4");
  CHECK_THAT(tester.getLine(), Contains("info string debug eval ="));
  CHECK_THAT(tester.getLine(), Contains("info"));
  CHECK_THAT(tester.getLine(), Contains("info"));
  CHECK_THAT(tester.getLine(), Contains("info"));
  CHECK_THAT(tester.getLine(), Contains("info") && Contains("pv g5g7 c7d8 h6h8"));
  CHECK_THAT(tester.getLine(), Contains("bestmove g5g7"));

  tester.putLine("quit");
  CHECK(tester.waitFor(std::chrono::milliseconds(100)) == std::future_status::ready);
}
