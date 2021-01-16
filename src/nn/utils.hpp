#pragma once

#include "../misc.hpp"
#include <config.hpp>
#include <immintrin.h>

namespace nn {

using Float = float;
inline constexpr size_t kFloatSize = sizeof(Float);
inline constexpr size_t kSimdWidth = 8; // TODO: Support SSE (float x 4)
inline constexpr size_t kFloatVectorSize = kFloatSize * kSimdWidth;

template<int N>
void relu(Float x[N], Float y[N]) {
  static_assert(N % 8 == 0);

  if constexpr (kUseAVX) {
    const __m256 kZero = {0, 0, 0, 0, 0, 0, 0, 0};
    for (int i = 0; i < N; i += 8) {
      auto v = _mm256_load_ps(&x[i]);
      v = _mm256_max_ps(v, kZero);
      _mm256_store_ps(&y[i], v);
    }

  } else {
    for (int i = 0; i < N; i++) {
      y[i] = std::max(x[i], 0.0f);
    }
  }
}

inline float sumSSE3(__m128 x) { // a b c d
  __m128 y = _mm_movehdup_ps(x); // b b d d
  x = _mm_add_ps(x, y);          // (a+b) (..) (c+d) (..)
  y = _mm_movehl_ps(x, x);       // (c+d) (..) (..)  (..)
  y = _mm_add_ss(x, y);          // (a+b+c+d) (..) (..) (..)
  return _mm_cvtss_f32(y);
}

inline float sumAVX(__m256 x) {
  __m128 lo = _mm256_castps256_ps128(x);
  __m128 hi = _mm256_extractf128_ps(x, 1);
  return sumSSE3(_mm_add_ps(lo, hi));
}

template<int N>
inline Float dot(Float x[N], Float y[N]) {
  static_assert(N % kSimdWidth == 0);

  if constexpr (kUseFMA) {
    __m256 res = {0, 0, 0, 0, 0, 0, 0, 0};
    for (int i = 0; i < N; i += 8) {
      auto vx = _mm256_load_ps(&x[i]);
      auto vy = _mm256_load_ps(&y[i]);
      res = _mm256_fmadd_ps(vx, vy, res);
    }
    return sumAVX(res);

  } else if constexpr (kUseAVX) {
    __m256 res = {0, 0, 0, 0, 0, 0, 0, 0};
    for (int i = 0; i < N; i += 8) {
      auto vx = _mm256_load_ps(&x[i]);
      auto vy = _mm256_load_ps(&y[i]);
      res = _mm256_add_ps(res, _mm256_mul_ps(vx, vy));
    }
    return sumAVX(res);

  } else {
    Float res = 0;
    for (int i = 0; i < N; i++) {
      res += x[i] * y[i];
    }
    return res;
  }
}

template<int N>
inline void copy(Float x[N], Float y[N]) {
  static_assert(N % kSimdWidth == 0);

  if constexpr (kUseAVX) {
    for (int i = 0; i < N; i += 8) {
      auto v = _mm256_load_ps(&x[i]);
      _mm256_store_ps(&y[i], v);
    }

  } else {
    for (int i = 0; i < N; i++) {
      y[i] = x[i];
    }
  }
}

template<int N>
inline void add(Float x[N], Float y[N], Float z[N]) {
  static_assert(N % kSimdWidth == 0);

  if constexpr (kUseAVX) {
    for (int i = 0; i < N; i += 8) {
      auto vx = _mm256_load_ps(&x[i]);
      auto vy = _mm256_load_ps(&y[i]);
      auto vz = _mm256_add_ps(vx, vy);
      _mm256_store_ps(&z[i], vz);
    }

  } else {
    for (int i = 0; i < N; i++) {
      z[i] = x[i] + y[i];
    }
  }
}

template<int N>
inline void sub(Float x[N], Float y[N], Float z[N]) {
  static_assert(N % kSimdWidth == 0);

  if constexpr (kUseAVX) {
    for (int i = 0; i < N; i += 8) {
      auto vx = _mm256_load_ps(&x[i]);
      auto vy = _mm256_load_ps(&y[i]);
      auto vz = _mm256_sub_ps(vx, vy);
      _mm256_store_ps(&z[i], vz);
    }

  } else {
    for (int i = 0; i < N; i++) {
      z[i] = x[i] - y[i];
    }
  }
}

template<int N1, int N2>
struct Linear {
  static_assert(N1 % kSimdWidth == 0);

  alignas(kFloatVectorSize) Float weight[N2][N1] = {};
  alignas(kFloatVectorSize) Float bias[N2] = {};

  void load(std::istream& istr) {
    istr.read(reinterpret_cast<char*>(weight[0]), N1 * N2 * kFloatSize);
    ASSERT(istr.gcount() == N1 * N2 * kFloatSize);
    istr.read(reinterpret_cast<char*>(bias), N2 * kFloatSize);
    ASSERT(istr.gcount() == N2 * kFloatSize);
  }

  void forward(float x[N1], float y[N2]) {
    for (int i = 0; i < N2; i++) {
      y[i] = dot<N1>(weight[i], x) + bias[i];
    }
  }
};

template<int N1, int N2>
struct InputLayer {
  static_assert(N2 % kSimdWidth == 0);

  alignas(kFloatVectorSize) Float weight[N1][N2] = {}; // Contiguous in output dimention
  alignas(kFloatVectorSize) Float bias[N2] = {};

  void load(std::istream& istr) {
    // NOTE: pytorch's embedding weight is already transposed
    istr.read(reinterpret_cast<char*>(weight[0]), N1 * N2 * kFloatSize);
    ASSERT(istr.gcount() == N1 * N2 * kFloatSize);
    istr.read(reinterpret_cast<char*>(bias), N2 * kFloatSize);
    ASSERT(istr.gcount() == N2 * kFloatSize);
  }
};

}; // namespace nn
