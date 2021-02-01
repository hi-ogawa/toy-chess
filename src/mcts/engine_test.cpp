#include "engine.hpp"
#include <config.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("mcts::Engine::go") {
  mcts::Engine engine;

  vector<SearchResult> results;
  engine.search_result_callback = [&](const SearchResult& result) { results.push_back(result); };

  if (kBuildType == "Release") {
    // TODO: When cpuct = 1, MCTS cannot even find mate in 2
    const auto kFenMateIn2 = "8/3k4/6R1/7R/8/4K3/8/8 w - - 2 2";
    engine.setFen(kFenMateIn2);
    engine.go_parameters.movetime = 1000;
    engine.go(/* blocking */ true);
    CHECK(toString(results.back()) != "bestmove h5h7");
  }
}
