#include "engine.hpp"
#include <config.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Engine::go") {
  precomputation::initializeTables();

  Engine engine;

  vector<SearchResult> results;
  engine.search_result_callback = [&](const SearchResult& result) { results.push_back(result); };

  string fen;
  int depth;
  string expected;
  const auto kFenMateIn2 = "8/3k4/6R1/7R/8/4K3/8/8 w - - 2 2";
  const auto kFenMateIn3 = "8/8/2k5/7R/6R1/4K3/8/8 w - - 0 1";

  if (kBuildType == "Release") {
    fen = kFenMateIn3;
    depth = 6;
    expected = "{g4g6, c6b7, h5h7, b7a8, g6g8, NONE}";
  } else {
    fen = kFenMateIn2;
    depth = 4;
    expected = "{h5h7, d7c8, g6g8, NONE}";
  }

  engine.position.initialize(fen);
  engine.go_parameters.depth = depth;
  engine.go(/* blocking */ true);

  auto bestmove = results.back();
  CHECK(bestmove.type == kSearchResultBestMove);
  CHECK(toString(bestmove.pv) == expected);
}
