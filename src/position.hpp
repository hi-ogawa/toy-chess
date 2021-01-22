#pragma once

#include "base.hpp"
#include "move.hpp"
#include "precomputation.hpp"
#include "transposition_table.hpp"
#include "nn/evaluator.hpp"

//
// Position
//

struct Position {
  array2<Board, 2, 6> pieces = {}; // pieces[color][piece_type]
  Color side_to_move = 0;
  int game_ply = 0;

  array<Board, 3> occupancy = {}; // white/black/both
  array2<PieceType, 2, 64> piece_on = {}; // 8x8 board representation

  // Irreversible state when make/unmake move
  struct State {
    array2<bool, 2, 2> castling_rights = {}; // castling_rights[color][side]
    Board ep_square = 0;
    int rule50 = 0;

    PieceType to_piece_type = kNoPieceType;
    Board checkers = 0;
    Board blockers = 0;

    Zobrist::Key key = 0;
  };

  State* state = nullptr;
  const static inline int kMaxDepth = 255;
  array<State, kMaxDepth + 1> state_stack;

  Position(const string& fen = kFenInitialPosition) {
    initialize(fen);
  }

  void initialize(const string& fen) {
    state = &state_stack[0];
    *state = {};
    setFen(fen);
    recompute(2);
  }

  // Reset internal stack etc...
  void reset() {
    initialize(toFen());
  }

  void pushState() {
    state++;
    *(state) = *(state - 1);
  }

  void popState() {
    state--;
  }

  void recompute(int, bool temporary = false);

  //
  // I/O
  //
  void setFen(const string&);
  string toFen() const;
  array2<char, 8, 8> toCharBoard() const;
  void printFen(std::ostream&) const;
  void print(std::ostream& ostr = std::cerr) const;
  friend std::ostream& operator<<(std::ostream& ostr, const Position& self) { self.print(ostr); return ostr; }

  //
  // Utility
  //
  Square kingSQ(Color color) const { return toSQ(pieces[color][kKing]).front(); }
  Board getAttackers(Color, Square, Board removed = 0) const;
  Board getBlockers(Color, Square) const;
  bool isPinned(Color, Square, Square, Square, Board) const;
  array<Board, 2> getPawnPush(Color) const;
  array<Board, 2> getPawnCapture(Color) const;

  //
  // Make/Unmake move
  //
  void putPiece(Color, PieceType, Square, bool temporary = false);
  void removePiece(Color, Square, bool temporary = false);
  void movePiece(Color, Square, Square, bool temporary = false);
  void makeMove(const Move&, bool temporary = false);
  void unmakeMove(const Move&, bool tempoary = false);
  void makeNullMove();
  void unmakeNullMove();

  //
  // Move generation
  //
  void generateMoves(MoveList&, MoveGenerationType movegen_type = kGenerateAll) const; // Generate pseudo legal moves
  void generatePawnPushMoves(MoveList&, Color, Board, MoveGenerationType) const;
  void generatePawnCaptureMoves(MoveList&, Color, Board, MoveGenerationType) const;
  bool isLegal(const Move& move) const; // Check legality of pseudo legal move
  bool isPseudoLegal(const Move& move) const; // Check pseudo legality of any move (used to validate tt move)

  //
  // Perft
  //
  int64_t perft(int, int debug = 0);
  vector<pair<Move, int64_t>> divide(int, int debug = 0);

  //
  // Evaluation
  //
  nn::Evaluator* evaluator = nullptr;

  // Score for side_to_move
  Score evaluate() const {
    ASSERT(evaluator);
    Score res = evaluator->evaluate();
    if (side_to_move == kBlack) { res *= -1; }
    res += kTempo;
    return res;
  }

  // Score for checkmate/stalemate
  Score evaluateLeaf(int depth) const {
    return state->checkers ? -Evaluation::mateScore(depth) : kScoreDraw;
  };

  //
  // Static exchange evaluation
  //
  Score evaluateMove(const Move&);
  Score computeSEE(Square);
  Move getLVA(Color, Square) const; // Least valuable attacker

  bool isCaptureOrPromotion(const Move&);
  bool givesCheck(const Move&);
};
