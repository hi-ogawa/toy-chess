
#include "base.hpp"
#include "precomputation.hpp"

//
// Move
//

enum MoveType { kNormal, kCastling, kPromotion, kEnpassant };

struct Move {
  Square from;
  Square to;
  MoveType type;
  PieceType promotion_type;
  CastlingSide castling_side;
  Move(Square from, Square to, MoveType type = kNormal) : from{from}, to{to}, type{type} {}

  Square getEnpassantCapturedSquare() const {
    assert(type == kEnpassant);
    return SQ::fromCoords(to % 8, from / 8);
  }

  void print(std::ostream& ostr) const {
    ostr << SQ(from) << SQ(to);
    if (type == kPromotion) { ostr << kFenPiecesMappingInverse[1][promotion_type]; }
  }

  friend std::ostream& operator<<(std::ostream& ostr, const Move& self) { self.print(ostr); return ostr; }
};

//
// Position
//

struct Position {
  array2<Board, 2, 6> pieces = {}; // pieces[color][piece_type]
  Color side_to_move = 0;
  int game_ply = 0;

  array<Board, 3> occupancy = {}; // white/black/both
  array2<PieceType, 2, 64> piece_on = {}; // 8x8 board representation
  Board checkers = 0;

  // Irreversible state when make/unmake move
  struct State {
    array2<bool, 2, 2> castling_rights = {}; // castling_rights[color][side]
    Board ep_square = 0;
    int rule50 = 0;
    PieceType to_piece_type = kNoPieceType;
  };

  State* state = nullptr;
  array<State, 256> state_stack;

  Position(const string& fen) {
    state = &state_stack[0];
    setFen(fen);
  }

  void pushState() {
    auto prev = state++;
    *state = *prev;
    state->to_piece_type = kNoPieceType;
  }

  void popState() { state--; }

  void recompute(int level = 0);

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
  bool isPinned(Color, Square, Square, Square, Board) const;
  array<Board, 2> getPawnPush(Color) const;

  //
  // Make/Unmake move
  //
  void putPiece(Color, PieceType, Square);
  void removePiece(Color, Square);
  void movePiece(Color, PieceType, Square, Square);
  void makeMove(const Move&);
  void unmakeMove(const Move&);

  //
  // Move generation
  //

  // TODO: Separate move generation for check evasion
  vector<Move> generateMoves() const;
  vector<Move> generateLegalMoves() const;
  bool isLegal(const Move& move) const;

  //
  // Perft
  //
  int64_t perft(int, int debug = 0);
  vector<pair<Move, int64_t>> divide(int, int debug = 0);
};
