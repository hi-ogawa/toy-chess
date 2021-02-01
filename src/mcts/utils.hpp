

namespace mcts {

//
// centi-pawn <=> win probability \in [0, 1]
//
inline float cp2win(int16_t cp) {
  return 1.0f / (1.0f + std::exp(- (cp / 100.0f)));
}

inline int16_t win2cp(float p) {
  p = std::min(std::max(cp2win(-10000), p), cp2win(+10000));
  return - std::log(1.0f / p - 1.0f) * 100;
}

//
// centi-pawn <=> reward \in [-1, 1]
//
inline float cp2reward(int16_t cp) {
  return 2.0f * cp2win(cp) - 1.0f;
}

inline int16_t reward2cp(float q) {
  return win2cp(((q + 1.0f) / 2.0f));
}

} // namespace mcts
