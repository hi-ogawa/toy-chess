#pragma once

#include "base.hpp"

using Score = int16_t;
const Score kScoreInf = 32001;
const Score kScoreMate = 32000;
const Score kScoreWin = 10000;
const Score kScoreDraw = 0;

//
// NOTE: Below are not used since evaluation is succeeded by nn::Evaluator
//

// Cf. https://www.chessprogramming.org/Simplified_Evaluation_Function by Tomasz Michniewski

// B > N > 3P
// B + N = R + 1.5P
// Q + P = 2R
const inline array<Score, 6> kPieceValue = {{
  100, // Pawn
  320, // Knight
  330, // Bishop
  500, // Rook
  900, // Queen
  20000, // King
}};

// NOTE: Rank index is flipped since this is copy-and-pasted from the page
const inline array2<Score, 6, 64> kPsqTable = {{
  // Pawn
  {{
      0,  0,  0,  0,  0,  0,  0,  0,
     50, 50, 50, 50, 50, 50, 50, 50,
     10, 10, 20, 30, 30, 20, 10, 10,
      5,  5, 10, 25, 25, 10,  5,  5,
      0,  0,  0, 20, 20,  0,  0,  0,
      5, -5,-10,  0,  0,-10, -5,  5,
      5, 10, 10,-20,-20, 10, 10,  5,
      0,  0,  0,  0,  0,  0,  0,  0
  }},
  // Knight
  {{
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50
  }},
  // Bishop
  {{
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20
  }},
  // Rook
  {{
      0,  0,  0,  0,  0,  0,  0,  0,
      5, 10, 10, 10, 10, 10, 10,  5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
      0,  0,  0,  5,  5,  0,  0,  0
  }},
  // Queen
  {{
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
     -5,  0,  5,  5,  5,  5,  0, -5,
      0,  0,  5,  5,  5,  5,  0, -5,
    -10,  5,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20
  }},
  // King
  {{
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -10,-20,-20,-20,-20,-20,-20,-10,
     20, 20,  0,  0,  0,  0, 20, 20,
     20, 30, 10,  0,  0, 10, 30, 20
  }},
}};

const inline Score kTempo = 10;

struct Evaluation {
  array<Score, 2> piece_value = {};
  array<Score, 2> psq = {};

  static inline Score mateScore(int ply) { return kScoreMate - ply; }

  Score value() const {
    Score res = 0;
    res += piece_value[kWhite] - piece_value[kBlack];
    res += psq[kWhite] - psq[kBlack];
    return res;
  }

  static inline Square toPsqCoord(Color color, Square sq) {
    // Flip rank for white
    return sq ^ ((1 - color) * (7 << 3));
  }

  void putPiece(Color color, PieceType type, Square to) {
    piece_value[color] += kPieceValue[type];
    psq[color] += kPsqTable[type][toPsqCoord(color, to)];
  }

  void removePiece(Color color, PieceType type, Square from) {
    piece_value[color] -= kPieceValue[type];
    psq[color] -= kPsqTable[type][toPsqCoord(color, from)];
  }
};
