
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
  Move() {}
  Move(Square from, Square to, MoveType type = kNormal) : from{from}, to{to}, type{type} {}

  Square getEnpassantCapturedSquare() const {
    assert(type == kEnpassant);
    return SQ::fromCoords(to % 8, from / 8);
  }

  void print(std::ostream& ostr = std::cerr) const {
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

  // Irreversible state when make/unmake move
  struct State {
    array2<bool, 2, 2> castling_rights = {}; // castling_rights[color][side]
    Board ep_square = 0;
    int rule50 = 0;

    PieceType to_piece_type = kNoPieceType;
    Board checkers = 0;
    Board blockers = 0;
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

    void initialize(Position* _position) {
      position = _position;
      first = last = 0;
    }

    void insert(const Move& move) { data[last++] = move; assert(last <= kMaxSize); }

    Move* getNext() {
      while (first < last) {
        auto move = &data[first++];
        if (position->isLegal(*move)) { return move; }
      }
      return nullptr;
    }

    vector<Move> toVector() {
      vector<Move> res;
      while (auto move = getNext()) { res.push_back(*move); }
      return res;
    }
  };

  MoveList* move_list;
  array<MoveList, kMaxDepth> move_list_stack;

  Position(const string& fen) {
    state = &state_stack[0];

    move_list = &move_list_stack[0];
    move_list->initialize(this);

    setFen(fen);
  }

  void pushState() {
    auto prev = state++;
    *state = *prev;
    state->to_piece_type = kNoPieceType;

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
  void movePiece(Color, PieceType, Square, Square);
  void makeMove(const Move&);
  void unmakeMove(const Move&);

  //
  // Move generation
  //

  void generateMoves();
  bool isLegal(const Move& move) const;

  //
  // Perft
  //
  int64_t perft(int, int debug = 0);
  vector<pair<Move, int64_t>> divide(int, int debug = 0);

  //
  // Evaluation (WIP)
  //
  using Score = float;
  const Score kScoreInf = 1e9;

  Score evaluate() const {
    return 0;
  }

  //
  // Search (WIP)
  //
  struct SearchResult {
    Score score = 0;
    vector<Move> moves;
  };

  SearchResult search(int depth) {
    SearchResult res;
    res.moves.resize(depth);
    searchImpl(-kScoreInf, kScoreInf, depth, &res.moves[0]);
    return res;
  }

  Score searchImpl(Score alpha, Score beta, int depth, Move* pv_move) {
    if (depth == 0) { return evaluate(); }
    while (auto move = move_list->getNext()) {
      makeMove(*move);
      auto score = -searchImpl(-beta, -alpha, depth - 1, pv_move + 1);
      unmakeMove(*move);
      if (beta <= score) { break; }
      if (alpha < score) { alpha = score; *pv_move = *move; }
    }
    return alpha;
  }
};
