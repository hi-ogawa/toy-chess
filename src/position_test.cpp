#include "position.hpp"
#include <config.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Position::print") {
  string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
  Position pos(fen);
  auto expected =
    "+---+---+---+---+---+---+---+---+\n"
    "| r | n | b | q | k | b | n | r | 8\n"
    "+---+---+---+---+---+---+---+---+\n"
    "| p | p | p | p | p | p | p | p | 7\n"
    "+---+---+---+---+---+---+---+---+\n"
    "|   |   |   |   |   |   |   |   | 6\n"
    "+---+---+---+---+---+---+---+---+\n"
    "|   |   |   |   |   |   |   |   | 5\n"
    "+---+---+---+---+---+---+---+---+\n"
    "|   |   |   |   |   |   |   |   | 4\n"
    "+---+---+---+---+---+---+---+---+\n"
    "|   |   |   |   |   |   |   |   | 3\n"
    "+---+---+---+---+---+---+---+---+\n"
    "| P | P | P | P | P | P | P | P | 2\n"
    "+---+---+---+---+---+---+---+---+\n"
    "| R | N | B | Q | K | B | N | R | 1\n"
    "+---+---+---+---+---+---+---+---+\n"
    "  a   b   c   d   e   f   g   h  \n"
    "\n"
    "Key: 0x3d46308f5f0dec7c\n"
    "Fen: rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1\n";
  CHECK(toString(pos) == expected);
}

TEST_CASE("Position::makeMove") {
  string fen =    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
  string fen_e4 = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1";
  Move move_e4 = Move(kE2, kE4);
  Position pos(fen);
  pos.makeMove(move_e4);
  CHECK(pos.toFen() == fen_e4);
  pos.unmakeMove(move_e4);
  CHECK(pos.toFen() == fen);
}

TEST_CASE("Position::generateMoves") {
  Position pos;
  MoveList move_list;
  pos.generateMoves(move_list);

  vector<Move> moves;
  for (auto [move, _score] : move_list) {
    if (pos.isLegal(move)) { moves.push_back(move); }
  }

  auto expected = "{a2a3, b2b3, c2c3, d2d3, e2e3, f2f3, g2g3, h2h3, a2a4, b2b4, c2c4, d2d4, e2e4, f2f4, g2g4, h2h4, b1a3, b1c3, g1f3, g1h3}";
  CHECK(toString(moves) == expected);
}

TEST_CASE("Position::isLegal") {
  Position pos;
  CHECK(pos.isPseudoLegal(Move(kA2, kA3)) == true);
  CHECK(pos.isLegal(Move(kA2, kA3)) == true);
}

TEST_CASE("Position::perft") {
  // Cf. https://www.chessprogramming.org/Perft_Results
  vector<pair<string, array<int64_t, 7>>> cases = {
    {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",             {{1, 20,  400,  8902,  197281,   4865609, 119060324}}},
    {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", {{1, 48, 2039, 97862, 4085603, 193690690, 8031647685}}},
    {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",                            {{1, 14,  191,  2812,   43238,    674624, 11030083}}},
    {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",     {{1,  6,  264,  9467,  422333,  15833292, 706045033}}},
    {"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",            {{1, 44, 1486, 62379, 2103487,  89941194}}}
  };

  const int max_depth = (kBuildType == "Release") ? 5 : 3;

  for (auto [fen, answer] : cases) {
    Position pos(fen);
    for (int depth = 0; depth <= max_depth; depth++) {
      CHECK(pos.perft(depth) == answer[depth]);
    }
  }
}

TEST_CASE("Position::divide") {
  Position pos(kFenInitialPosition);
  pos.divide(3);
  auto expected = "{(a2a3, 380), (b2b3, 420), (c2c3, 420), (d2d3, 539), (e2e3, 599), (f2f3, 380), (g2g3, 420), (h2h3, 380), (a2a4, 420), (b2b4, 421), (c2c4, 441), (d2d4, 560), (e2e4, 600), (f2f4, 401), (g2g4, 421), (h2h4, 420), (b1a3, 400), (b1c3, 440), (g1f3, 440), (g1h3, 400)}";
  CHECK(toString(pos.divide(3)) == expected);
}

TEST_CASE("Position::State::key") {
  Position pos;
  auto key1 = pos.state->key;
  pos.makeMove(Move(kE2, kE4));
  auto key2 = pos.state->key;
  pos.unmakeMove(Move(kE2, kE4));
  auto key3 = pos.state->key;
  CHECK(key1 != key2);
  CHECK(key1 == key3);
}
