#include "engine.hpp"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Engine::search") {
  precomputation::initializeTables();

  Position position("8/8/2k5/7R/6R1/4K3/8/8 w - - 0 1");
  Engine engine;
  auto res = engine.search(position, 6);
  auto expected = "(2, 6, 31995, {g4g6, c6b7, h5h7, b7a8, g6g8, NONE})";
  CHECK(toString(res) == expected);
}

TEST_CASE("Engine::go") {
  precomputation::initializeTables();

  Engine engine;
  vector<SearchResult> results;
  engine.search_result_callback = [&](const SearchResult& result) { results.push_back(result); };

  Position position("8/8/2k5/7R/6R1/4K3/8/8 w - - 0 1");
  engine.go(position, 6);

  vector<string> expected = {{
    "(0, 1, 1005, {e3d2})",
    "(0, 2, 1015, {g4g7, c6b6})",
    "(0, 3, 1015, {g4g7, c6b6, e3d2})",
    "(0, 4, 1025, {g4g7, c6b6, e3d2, b6a6})",
    "(0, 5, 1035, {g4g7, c6b6, e3f2, b6a6, f2g1})",
    "(0, 6, 31995, {g4g6, c6b7, h5h7, b7a8, g6g8, NONE})",
    "(1, 6, 31995, {g4g6, c6b7, h5h7, b7a8, g6g8, NONE})"
  }};

  for (int i = 0; i < 7; i++) {
    CHECK(toString(results[i]) == expected[i]);
  }
}
