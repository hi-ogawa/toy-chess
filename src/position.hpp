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
  const static inline int kMaxDepth = 256;
  array<State, kMaxDepth> state_stack;

  // Fixed size move list with legality filtering
  struct MoveList {
    Position* position;
    const static inline int kMaxSize = 256;
    array<Move, kMaxSize> data = {};
    int first = 0, last = 0;
    int cnt = 0;

    void initialize(Position* _position) {
      position = _position;
      first = last = cnt = 0;
    }

    void insert(const Move& move) { data[last++] = move; assert(last <= kMaxSize); }

    Move* getNext() {
      while (first < last) {
        auto move = &data[first++];
        if (position->isLegal(*move)) { cnt++; return move; }
      }
      return nullptr;
    }

    int size() { assert(first == last); return cnt; } // Must be called after "getNext" exhausted legal moves

    vector<Move> toVector() {
      vector<Move> res;
      while (auto move = getNext()) { res.push_back(*move); }
      return res;
    }
  };

  MoveList* move_list;
  array<MoveList, kMaxDepth> move_list_stack;

  Position(const string& fen = kFenInitialPosition) {
    initialize(fen);
  }

  void initialize(const string& fen) {
    state = &state_stack[0];
    *state = {};

    move_list = &move_list_stack[0];
    move_list->initialize(this);

    setFen(fen);

    recompute(2);
  }

  // Reset internal stack etc...
  void reset() {
    initialize(toFen());
  }

  void pushState() {
    auto prev = state;
    *(++state) = *prev;
    (++move_list)->initialize(this);
  }

  void popState() {
    state--;
    move_list--;
  }

  void recompute(int);

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

  //
  // Make/Unmake move
  //
  void putPiece(Color, PieceType, Square);
  void removePiece(Color, Square);
  void movePiece(Color, Square, Square);
  void makeMove(const Move&);
  void unmakeMove(const Move&);

  //
  // Move generation
  //

  void generateMoves(bool only_capture = 0); // Generate pseudo legal moves
  bool isLegal(const Move& move) const; // Check legality of pseudo legal move
  bool isPseudoLegal(const Move& move) const; // Check pseudo legality of any move (used to validate tt move)
  void generatePawnCaptureMoves(Color);

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
};
