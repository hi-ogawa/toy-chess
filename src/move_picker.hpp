#include "move.hpp"
#include "position.hpp"

enum MovePickerStage {
  // Default
  kTTMoveStage,
  kInitCaptureStage,
  kGoodCaptureStage,
  kRefutationStage,
  kInitQuietStage,
  kQuietStage,
  kBadCaptureStage,
  kEndStage,

  // Quiescence
  kQuiescenceTTMoveStage,
  kQuiescenceInitCaptureStage,
  kQuiescenceCaptureStage,
  kQuiescenceCheckStage,
  kQuiescenceEndStage,

  // Evasion
  kEvasionTTMoveStage,
  kEvasionInitStage,
  kEvasionStage,
  kEvastionEndStage
};

using MoveScore = std::pair<Move, Score>;

using MoveScoreList = SimpleQueue<MoveScore, 256>;

void sortMoveScoreList(MoveScoreList& list) {
  std::sort(list.begin(), list.end(), [](auto x, auto y) { return x.second > y.second; });
}

struct MovePicker {
  Position& position;
  History& history;
  array<Move, 2> killers;
  MovePickerStage stage;

  Move tt_move;
  MoveList refutations;

  MoveScoreList good_captures;
  MoveScoreList bad_captures;
  MoveScoreList quiets;

  MoveList tmp_list;

  MovePicker(Position& p, History& h, Move tt_move, array<Move, 2> killers, bool in_check, bool quiescence)
    : position{p}, history{h}, killers{killers}, tt_move{tt_move} {

    if (in_check) {
      stage = kEvasionTTMoveStage;
    } else if (quiescence) {
      stage = kQuiescenceTTMoveStage;
    } else {
      stage = kTTMoveStage;
    }
  }

  bool getNext(Move& res_move) {
    tmp_list.clear();

    //
    // Default
    //
    if (stage == kTTMoveStage) {
      stage = kInitCaptureStage;
      if (position.isPseudoLegal(tt_move) && position.isLegal(tt_move)) {
        res_move = tt_move;
        return true;
      }
    }

    if (stage == kInitCaptureStage) {
      position.generateMoves(tmp_list, kGenerateCapture);
      for (auto move : tmp_list) {
        if (move != tt_move && position.isLegal(move)) {
          auto score = history.getCaptureScore(position, move);
          auto see_score = position.evaluateMove(move);
          if (see_score >= 0) {
            good_captures.put({move, score});
          } else {
            bad_captures.put({move, score});
          }
        }
      }
      sortMoveScoreList(good_captures);
      stage = kGoodCaptureStage;
    }

    if (stage == kGoodCaptureStage) {
      while (!good_captures.empty()) { res_move = good_captures.get().first; return true; }

      stage = kRefutationStage;
      for (auto move : killers) {
        if (move != tt_move && position.isPseudoLegal(move) && position.isLegal(move)) {
          refutations.put(move);
        }
      }
    }

    if (stage == kRefutationStage) {
      while (!refutations.empty()) { res_move = refutations.get(); return true; }
      stage = kInitQuietStage;
    }

    if (stage == kInitQuietStage) {
      position.generateMoves(tmp_list, kGenerateQuiet);
      for (auto move : tmp_list) {
        if (move != tt_move && position.isLegal(move)) {
          auto score = history.getQuietScore(position, move);
          quiets.put({move, score});
        }
      }
      sortMoveScoreList(quiets);
      stage = kQuietStage;
    }

    if (stage == kQuietStage) {
      while (!quiets.empty()) { res_move = quiets.get().first; return true; }
      stage = kBadCaptureStage;
    }

    if (stage == kBadCaptureStage) {
      while (!bad_captures.empty()) { res_move = bad_captures.get().first; return true; }
      stage = kEndStage;
    }

    if (stage == kEndStage) { return false; }

    //
    // Quiescence
    //

    if (stage == kQuiescenceTTMoveStage) {
      stage = kQuiescenceInitCaptureStage;
      if (position.isPseudoLegal(tt_move) && position.isLegal(tt_move)) {
        res_move = tt_move;
        return true;
      }
    }

    if (stage == kQuiescenceInitCaptureStage) {
      position.generateMoves(tmp_list, kGenerateCapture);
      for (auto move : tmp_list) {
        if (move != tt_move && position.isLegal(move)) {
          auto score = history.getCaptureScore(position, move);
          good_captures.put({move, score});
        }
      }
      sortMoveScoreList(good_captures);
      stage = kQuiescenceCaptureStage;
    }

    if (stage == kQuiescenceCaptureStage) {
      while (!good_captures.empty()) { res_move = good_captures.get().first; return true; }
      stage = kQuiescenceCheckStage;
    }

    if (stage == kQuiescenceCheckStage) {
      // TODO
      stage = kQuiescenceEndStage;
    }

    if (stage == kQuiescenceEndStage) { return false; }

    //
    // Evasion
    //

    if (stage == kEvasionTTMoveStage) {
      stage = kEvasionInitStage;
      if (position.isPseudoLegal(tt_move) && position.isLegal(tt_move)) {
        res_move = tt_move;
        return true;
      }
    }

    if (stage == kEvasionInitStage) {
      position.generateMoves(tmp_list, kGenerateAll);
      for (auto move : tmp_list) {
        if (move != tt_move && position.isLegal(move)) {
          auto score = position.isCaptureOrPromotion(move)
            ? history.getCaptureScore(position, move)
            : history.getQuietScore(position, move);
          good_captures.put({move, score});
        }
      }
      sortMoveScoreList(good_captures);
      stage = kEvasionStage;
    }

    if (stage == kEvasionStage) {
      while (!good_captures.empty()) { res_move = good_captures.get().first; return true; }
      stage = kEvastionEndStage;
    }

    if (stage == kEvastionEndStage) { return false; }

    ASSERT(0);
    return false;
  }
};
