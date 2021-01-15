#include "position.hpp"
#include "nn/evaluator.hpp"

using namespace precomputation;

void Position::recompute(int level) {
  // init
  if (level >= 2) {
    fillArray(piece_on, kNoPieceType);
    for (Color color = 0; color < 2; color++) {
      occupancy[color] = 0;
      for (PieceType type = 0; type < 6; type++) {
        occupancy[color] |= pieces[color][type];
        for (auto sq : toSQ(pieces[color][type])) {
          piece_on[color][sq] = type;
          state->key ^= Zobrist::piece_squares[color][type][sq];
        }
      }
    }
    if (side_to_move) {
      state->key ^= Zobrist::side_to_move;
    }
    if (state->ep_square) {
      state->key ^= Zobrist::ep_squares[toSQ(state->ep_square).front()];
    }
    for (Color i = 0; i < 2; i++) {
      for (CastlingSide j = 0; j < 2; j++) {
        if (state->castling_rights[i][j]) {
          state->key ^= Zobrist::castling_rights[i][j];
        }
      }
    }
    if (evaluator) { evaluator->initialize(*this); }
  }

  // init, makeMove, unmakeMove
  occupancy[kBoth] = occupancy[kWhite] | occupancy[kBlack];

  // init, makeMove
  if (level >= 1) {
    state->checkers = getAttackers(side_to_move, kingSQ(side_to_move));
    state->blockers = getBlockers(side_to_move, kingSQ(side_to_move));
  }
}

//
// FEN convertion
//

void Position::setFen(const string& fen) {
  std::istringstream fen_istr(fen);
  string s_board, s_side_to_move, s_castling_rights, s_ep_square, s_halfmove_clock, s_fullmove_counter;
  fen_istr >> s_board >> s_side_to_move >> s_castling_rights >> s_ep_square >> s_halfmove_clock >> s_fullmove_counter;

  // Pieces
  {
    pieces = {};
    int i = 7, j = 0;
    for (auto c : s_board) {
      if (c == '/') { i--; j = 0; continue; }
      if ('1' <= c && c <= '8') { j += (c - '0'); continue; }
      assert(kFenPiecesMapping.count(c));
      assert(0 <= i && i < 8 && 0 <= j && j < 8);
      auto [color, type] = kFenPiecesMapping[c];
      pieces[color][type] |= toBB(SQ::fromCoords(j, i));
      j++;
    }
  }

  // Side to move
  assert(s_side_to_move == "w" || s_side_to_move == "b");
  side_to_move = (s_side_to_move == "w") ? kWhite : kBlack;

  // Castling
  state->castling_rights = {};
  if (s_castling_rights != "-") {
    for (auto c : s_castling_rights) {
      if (c == 'K') { state->castling_rights[kWhite][kOO] = 1; }
      if (c == 'Q') { state->castling_rights[kWhite][kOOO] = 1; }
      if (c == 'k') { state->castling_rights[kBlack][kOO] = 1; }
      if (c == 'q') { state->castling_rights[kBlack][kOOO] = 1; }
    }
  }

  // En passant
  state->ep_square = 0;
  if (s_ep_square != "-") {
    assert((int)s_ep_square.size() == 2);
    int j = s_ep_square[0] - 'a';
    int i = s_ep_square[1] - '1';
    assert(kFileA <= j && j <= kFileH);
    assert((side_to_move == kWhite && i == kRank6) || (side_to_move == kBlack && i == kRank3));
    state->ep_square = toBB(SQ::fromCoords(j, i));
  }

  // Counter
  state->rule50 = std::stoi(s_halfmove_clock);
  game_ply = 2 * (std::stoi(s_fullmove_counter) - 1) + (side_to_move == kBlack);
}

array2<char, 8, 8> Position::toCharBoard() const {
  array2<char, 8, 8> res;
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      auto sq = SQ::fromCoords(j, i);
      char mark = ' ';
      for (int color = 0; color < 2; color++) {
        if (occupancy[color] & toBB(sq)) {
          mark = kFenPiecesMappingInverse[color][piece_on[color][sq]];
        }
      }
      res[i][j] = mark;
    }
  }
  return res;
}

string Position::toFen() const {
  std::ostringstream ostr;
  printFen(ostr);
  return ostr.str();
}

void Position::printFen(std::ostream& ostr) const {
  // Pieces
  auto char_board = toCharBoard();
  for (int i = 7; i >= 0; i--) {
    int cnt = 0;
    for (int j = 0; j < 8; j++) {
      char piece = char_board[i][j];
      if (piece == ' ') { cnt++; continue; }
      if (cnt) { ostr << cnt; cnt = 0; }
      ostr << piece;
    }
    if (cnt) { ostr << cnt; }
    if (i != 0) { ostr << "/"; }
  }
  ostr << " ";

  // Side to move
  ostr << (side_to_move ? "b" : "w") << " ";

  // Castling
  string s_castling;
  for (int color = 0; color < 2; color++) {
    for (int side = 0; side < 2; side++) {
      if (state->castling_rights[color][side]) {
        s_castling += ("KQ"[side] + (color) * ('a' - 'A'));
      }
    }
  }
  ostr << (s_castling.empty() ? "-" : s_castling) << " ";

  // En passant
  string s_ep_square;
  if (state->ep_square) {
    assert(toSQ(state->ep_square).size() == 1);
    auto [file, rank] = SQ::toCoords(toSQ(state->ep_square).front());
    s_ep_square += ('a' + file);
    s_ep_square += ('1' + rank);
  }
  ostr << (s_ep_square.empty() ? "-" : s_ep_square) << " ";

  // Counter
  ostr << state->rule50 << " ";
  ostr << ((game_ply / 2) + 1);
}

void Position::print(std::ostream& ostr) const {
  auto char_board = toCharBoard();
  for (int i = 7; i >= 0; i--) {
    ostr << "+---+---+---+---+---+---+---+---+" << "\n";
    for (int j = 0; j < 8; j++) {
      ostr << "| " << char_board[i][j] << " ";
    }
    ostr << "| " << (i + 1) << "\n";
  }
  ostr << "+---+---+---+---+---+---+---+---+" << "\n";
  ostr << "  a   b   c   d   e   f   g   h  " << "\n";
  ostr << "\n";
  ostr << "Key: "; Zobrist::print(state->key, ostr); ostr << "\n";
  ostr << "Fen: "; printFen(ostr);
  ostr << "\n";
}


//
// Utility
//

Board Position::getAttackers(Color attacked, Square to, Board removed) const {
  Board occ = occupancy[kBoth] & ~removed;
  return
    (pawn_attack_table[attacked][to] & pieces[!attacked][kPawn]) |
    (king_attack_table[to]           & pieces[!attacked][kKing]) |
    (knight_attack_table[to]         & pieces[!attacked][kKnight]) |
    (getRookAttack(to, occ)   & (pieces[!attacked][kRook]   | pieces[!attacked][kQueen])) |
    (getBishopAttack(to, occ) & (pieces[!attacked][kBishop] | pieces[!attacked][kQueen]));
}

Board Position::getBlockers(Color own, Square to) const {
  Board res(0);
  Board rooks = getRookAttack(to, Board(0)) & (pieces[!own][kRook] | pieces[!own][kQueen]);
  Board bishops = getBishopAttack(to, Board(0)) & (pieces[!own][kBishop] | pieces[!own][kQueen]);
  for (auto from : toSQ(rooks)) {
    Board b = in_between_table[from][to] & occupancy[kBoth];
    if (toSQ(b).size() == 1) { res |= b; }
  }
  for (auto from : toSQ(bishops)) {
    Board b = in_between_table[from][to] & occupancy[kBoth];
    if (toSQ(b).size() == 1) { res |= b; }
  }
  return res;
}

bool Position::isPinned(Color own, Square sq, Square from, Square to, Board removed) const {
  if (SQ::isAligned(sq, from, to)) { return 0; }

  auto [dir, ray_type] = SQ::getRayType(sq, from);
  if (ray_type == kNoRayType) { return 0; }

  Board ray = getRay(sq, dir, occupancy[kBoth] & ~removed);
  if (ray_type == kRookRay)   { return ray & (pieces[!own][kRook]   | pieces[!own][kQueen]); }
  if (ray_type == kBishopRay) { return ray & (pieces[!own][kBishop] | pieces[!own][kQueen]); }
  return 0;
}

array<Board, 2> Position::getPawnPush(Color own) const {
  Board push1, push2;
  if (own == kWhite) {
    push1 = (pieces[own][kPawn] << 8) & ~occupancy[kBoth];
    push2 = (push1 << 8) & BB::fromRank(kRank4) & ~occupancy[kBoth];
  } else {
    push1 = (pieces[own][kPawn] >> 8) & ~occupancy[kBoth];
    push2 = (push1 >> 8) & BB::fromRank(kRank5) & ~occupancy[kBoth];
  }
  return {push1, push2};
}

//
// Make/unmake move
//

void Position::putPiece(Color color, PieceType type, Square sq) {
  assert(piece_on[color][sq] == kNoPieceType);
  pieces[color][type] ^= toBB(sq);
  occupancy[color] ^= toBB(sq);
  piece_on[color][sq] = type;
  state->key ^= Zobrist::piece_squares[color][type][sq];
  if (evaluator) { evaluator->putPiece(color, type, sq); }
}

void Position::removePiece(Color color, Square sq) {
  auto type = piece_on[color][sq];
  assert(type != kNoPieceType);
  pieces[color][type] ^= toBB(sq);
  occupancy[color] ^= toBB(sq);
  piece_on[color][sq] = kNoPieceType;
  state->key ^= Zobrist::piece_squares[color][type][sq];
  if (evaluator) { evaluator->removePiece(color, type, sq); }
}

void Position::movePiece(Color color, Square from, Square to) {
  auto type = piece_on[color][from];
  removePiece(color, from);
  putPiece(color, type, to);
}

void Position::makeMove(const Move& move) {
  // Copy irreversible state
  pushState();

  Color own = side_to_move, opp = !own;
  auto from_type = piece_on[own][move.from()];
  auto to_type = state->to_piece_type = piece_on[opp][move.to()];
  assert(from_type != kNoPieceType);

  //
  // put/remove/move pieces
  //

  if (to_type != kNoPieceType) { // Capture except en passant (thus, this includes promotion)
    assert(to_type != kKing);
    removePiece(opp, move.to());
  }

  if (move.type() == kNormal) {
    movePiece(own, move.from(), move.to());
  }

  if (move.type() == kCastling) {
    auto [king_from, king_to, rook_from, rook_to] = kCastlingMoves[own][move.castlingSide()];
    assert(piece_on[own][king_from] == kKing      && piece_on[own][rook_from] == kRook);
    assert(piece_on[own][king_to] == kNoPieceType && piece_on[own][rook_to] == kNoPieceType);
    movePiece(own, king_from, king_to);
    movePiece(own, rook_from, rook_to);
  }

  if (move.type() == kPromotion) {
    removePiece(own, move.from());
    putPiece(own, move.promotionType(), move.to());
  }

  if (move.type() == kEnpassant) {
    auto sq = move.capturedPawnSquare();
    assert(piece_on[opp][sq] == kPawn);
    movePiece(own, move.from(), move.to());
    removePiece(opp, sq);
  }

  //
  // Castling rights
  //
  if (from_type == kKing) {
    for (auto side : {kOO, kOOO}) {
      if (state->castling_rights[own][side]) {
        state->castling_rights[own][side] = 0;
        state->key ^= Zobrist::castling_rights[own][side];
      }
    }
  }
  if (from_type == kRook) {
    for (auto side : {kOO, kOOO}) {
      if (state->castling_rights[own][side] && move.from() == kCastlingMoves[own][side][2]) {
        state->castling_rights[own][side] = 0;
        state->key ^= Zobrist::castling_rights[own][side];
      }
    }
  }
  if (to_type == kRook) {
    for (auto side : {kOO, kOOO}) {
      if (state->castling_rights[opp][side] && move.to() == kCastlingMoves[opp][side][2]) {
        state->castling_rights[opp][side] = 0;
        state->key ^= Zobrist::castling_rights[opp][side];
      }
    }
  }

  //
  // En passant square
  //
  if (from_type == kPawn && std::abs(move.to() - move.from()) == 2 * kDirN) {
    Square sq = (move.to() + move.from()) / 2;
    state->ep_square = toBB(sq);
    state->key ^= Zobrist::ep_squares[sq];
  } else {
    state->ep_square = 0;
  }

  //
  // 50 move rule
  //
  state->rule50++;
  if ((to_type != kNoPieceType) || (from_type == kPawn) || (move.type() != kNormal)) {
    state->rule50 = 0;
  }

  // Reversible states
  side_to_move = opp;
  game_ply++;
  state->key ^= Zobrist::side_to_move;

  // Recompute states
  recompute(1);

  // Reset evaluator on king move
  if (from_type == kKing && evaluator) {
    evaluator->initialize(*this);
  }
}

void Position::unmakeMove(const Move& move) {
  // Reversible states
  side_to_move = !side_to_move;
  game_ply--;

  Color own = side_to_move, opp = !own;
  auto from_type = piece_on[own][move.to()];
  auto to_type = state->to_piece_type;
  assert(from_type != kNoPieceType);

  //
  // put/remove/move pieces
  //

  if (to_type != kNoPieceType) {
    assert(to_type != kKing);
    putPiece(opp, to_type, move.to());
  }

  if (move.type() == kNormal) {
    movePiece(own, move.to(), move.from());
  }

  if (move.type() == kCastling) {
    auto [king_from, king_to, rook_from, rook_to] = kCastlingMoves[own][move.castlingSide()];
    movePiece(own, king_to, king_from);
    movePiece(own, rook_to, rook_from);
  }

  if (move.type() == kPromotion) {
    removePiece(own, move.to());
    putPiece(own, kPawn, move.from());
  }

  if (move.type() == kEnpassant) {
    auto sq = move.capturedPawnSquare();
    putPiece(opp, kPawn, sq);
    movePiece(own, move.to(), move.from());
  }

  // Restore irreversible state (castling rights, en passant square, rule50)
  popState();

  // Recompute states
  recompute(0);

  // Reset evaluator on king move
  if (from_type == kKing && evaluator) {
    evaluator->initialize(*this);
  }
}


//
// Move generation
//

void Position::generateMoves() {
  move_list->initialize(this);

  Color own = side_to_move, opp = !own;
  Board& occ = occupancy[kBoth];
  Board& own_occ = occupancy[own];
  Board& opp_occ = occupancy[opp];
  Square king_sq = kingSQ(own);

  bool checked = state->checkers;
  bool multiple_checked = toSQ(state->checkers).size() >= 2;
  Board target = 0;
  if (!checked) {
    // Quiet or Capture
    target = ~own_occ;
  }
  if (checked && !multiple_checked) {
    // Block or Capture single checker
    Square checker_sq = toSQ(state->checkers).front();
    target = in_between_table[king_sq][checker_sq] | state->checkers;
  }

  if (!multiple_checked) {
    // Pawn
    {
      // Push
      auto [push1, push2] = getPawnPush(own);
      push1 &= target;
      push2 &= target;
      for (auto to : toSQ(push1 & kBackrankBB[opp])) { // Promotion
        auto from = to - kPawnPushDirs[own];
        for (auto type : {kKnight, kBishop, kRook, kQueen}) {
          Move move(from, to, kPromotion, type);
          move_list->insert(move);
        }
      }
      for (auto to : toSQ(push1 & ~kBackrankBB[opp])) { // Non-promotion
        auto from = to - kPawnPushDirs[own];
        move_list->insert(Move(from, to));
      }
      for (auto to : toSQ(push2)) { // Double push
        auto from = to - 2 * kPawnPushDirs[own];
        move_list->insert(Move(from, to));
      }

      for (auto from : toSQ(pieces[own][kPawn])) {
        // Capture
        Board capture = pawn_attack_table[own][from] & opp_occ & target;
        for (auto to : toSQ(capture & kBackrankBB[opp])) { // Promotion
          for (auto type : {kKnight, kBishop, kRook, kQueen}) {
            Move move(from, to, kPromotion, type);
            move_list->insert(move);
          }
        }
        for (auto to : toSQ(capture & ~kBackrankBB[opp])) { // Non-promotion
          move_list->insert(Move(from, to));
        }

        // En passant
        if (pawn_attack_table[own][from] & state->ep_square) {
          auto to = toSQ(state->ep_square).front();
          move_list->insert(Move(from, to, kEnpassant));
        }
      }
    }

    // Knight
    for (auto from : toSQ(pieces[own][kKnight])) {
      for (auto to : toSQ(knight_attack_table[from] & target)) {
        move_list->insert(Move(from, to));
      }
    }

    // Bishop
    for (auto from : toSQ(pieces[own][kBishop])) {
      for (auto to : toSQ(getBishopAttack(from, occ) & target)) {
        move_list->insert(Move(from, to));
      }
    }

    // Rook
    for (auto from : toSQ(pieces[own][kRook])) {
      for (auto to : toSQ(getRookAttack(from, occ) & target)) {
        move_list->insert(Move(from, to));
      }
    }

    // Queen
    for (auto from : toSQ(pieces[own][kQueen])) {
      for (auto to : toSQ(getQueenAttack(from, occ) & target)) {
        move_list->insert(Move(from, to));
      }
    }
  }

  // King
  for (auto from : toSQ(pieces[own][kKing])) {
    for (auto to : toSQ(king_attack_table[from] & ~own_occ)) {
      move_list->insert(Move(from, to));
    }
  }

  // Castling
  if (!checked) {
    for (auto side : {kOO, kOOO}) {
      if (!state->castling_rights[own][side]) { continue; }
      auto [king_from, king_to, rook_from, rook_to] = kCastlingMoves[own][side];
      if (in_between_table[king_from][rook_from] & occ) { continue; }
      Move move(king_from, king_to, kCastling);
      move_list->insert(move);
    }
  }
}

bool Position::isLegal(const Move& move) const {
  Color own = side_to_move;

  Square king_sq = kingSQ(own);
  bool is_king_move = (piece_on[own][move.from()] == kKing);

  //
  // Check evasion
  //

  if (state->checkers) {
    int num_checkers = toSQ(state->checkers).size();

    if (move.type() == kCastling) { return 0; }

    // King's move
    if (is_king_move) { return !getAttackers(own, move.to(), toBB(move.from())); }

    // Multiple check but not king move
    if (num_checkers >= 2) { return 0; }

    // Single check
    auto checker_sq = toSQ(state->checkers).front();
    if (isPinned(own, king_sq, move.from(), move.to(), toBB(move.from()))) { return 0; } // TODO: Can we use "state->blockers"?
    if (move.to() == checker_sq) { return 1; } // capture
    if (move.type() == kEnpassant && checker_sq == move.capturedPawnSquare()) { return 1; } // Enpassant capture
    if (in_between_table[king_sq][checker_sq] & toBB(move.to())) { return 1; } // blocking
    return 0;
  }

  //
  // Non check evasion
  //

  // Castling path is attacked
  if (move.type() == kCastling) {
    auto [king_from, king_to, _0, _1] = kCastlingMoves[own][move.castlingSide()];
    for (auto sq : toSQ(in_between_table[king_from][king_to] | toBB(king_to))) {
      if (getAttackers(own, sq)) { return 0; }
    }
    return 1;
  }

  // Pinned enpassant
  if (move.type() == kEnpassant) {
    Square opp_sq = move.capturedPawnSquare();
    Board removed = toBB(move.from()) | toBB(opp_sq);
    if (isPinned(own, king_sq, move.from(), move.to(), removed)) { return 0; }
    if (isPinned(own, king_sq, opp_sq,      move.to(), removed)) { return 0; }
    return 1;
  }

  // King's move
  if (is_king_move) { return !getAttackers(own, move.to()); }

  // Pinned piece move
  if (state->blockers & toBB(move.from())) {
    return SQ::isAligned(king_sq, move.from(), move.to());
  }

  return 1;
}

int64_t Position::perft(int depth, int debug) {
  assert(depth >= 0);
  if (depth == 0) { return 1; }
  int64_t res = 0;
  generateMoves();
  while (auto move = move_list->getNext()) {
    if (depth == 1) { res++; continue; }
    makeMove(*move);
    if (debug >= 1) { dbg(depth, *move); }
    if (debug >= 2) { print(); }
    res += perft(depth - 1, debug);
    unmakeMove(*move);
  }
  return res;
}

vector<pair<Move, int64_t>> Position::divide(int depth, int debug) {
  assert(depth >= 1);
  vector<pair<Move, int64_t>> res;
  generateMoves();
  while (auto move = move_list->getNext()) {
    makeMove(*move);
    if (debug >= 1) { dbg(depth, *move); }
    if (debug >= 2) { print(); }
    res.push_back({*move, perft(depth - 1, debug)});
    unmakeMove(*move);
  }
  return res;
}
