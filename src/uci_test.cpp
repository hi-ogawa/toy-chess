#include "uci.hpp"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("UCI") {
  precomputation::initializeTables();

  // TODO: How to actually wait for result without quitting?
  auto input = "";
  auto ostr_expected = "";

  std::istringstream istr(input);
  std::ostringstream ostr, err_ostr;

  UCI uci(istr, ostr, err_ostr);
  uci.mainLoop();

  CHECK(ostr.str() == ostr_expected);
}
