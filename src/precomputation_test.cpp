
#include "precomputation.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace precomputation;

TEST_CASE("precomputation::getRay") {
  generateDistanceTable();
  auto expected =
    "X.......\n"
    ".X......\n"
    "..X.....\n"
    "...X....\n"
    "....X...\n"
    ".....X..\n"
    "........\n"
    "........\n";
  CHECK(BB(getRay(kG2, kDirNW, 0)).toString(0) == expected);
}

TEST_CASE("precomputation::getQueenAttack") {
  initializeTables();

  Square from = kD2;
  Board occ = toBB(kB4) | toBB(kE3) | toBB(kA2) | toBB(kD6);
  Board b = getQueenAttack(from, occ);
  auto expected =
    "........\n"
    "........\n"
    "...X....\n"
    "...X....\n"
    ".X.X....\n"
    "..XXX...\n"
    "XXX.XXXX\n"
    "..XXX...\n";

  CHECK(BB(b).toString(0) == expected);
}

TEST_CASE("precomputation::generateNonSlidingAttackTables") {
  generateDistanceTable();
  generateNonSlidingAttackTables();
  Square from = kD2;
  {
    auto expected =
      "........\n"
      "........\n"
      "........\n"
      "........\n"
      "..X.X...\n"
      ".X...X..\n"
      "........\n"
      ".X...X..\n";
    CHECK(BB(knight_attack_table[from]).toString(0) == expected);
  }
  {
    auto expected =
      "........\n"
      "........\n"
      "........\n"
      "........\n"
      "........\n"
      "..XXX...\n"
      "..X.X...\n"
      "..XXX...\n";
    CHECK(BB(king_attack_table[from]).toString(0) == expected);
  }
  {
    auto expected =
      "........\n"
      "........\n"
      "........\n"
      "........\n"
      "........\n"
      "..X.X...\n"
      "........\n"
      "........\n";
    CHECK(BB(pawn_attack_table[kWhite][from]).toString(0) == expected);
  }
}
