#pragma once

#include "../misc.hpp"
#include <config.hpp>
#include <immintrin.h>

namespace nn {

using Float = float;
inline constexpr size_t kFloatSize = sizeof(Float);
inline constexpr size_t kMaxSimdWidth = 8;
inline constexpr size_t kMaxFloatVectorSize = kFloatSize * kMaxSimdWidth;

template<int N>
void relu(Float x[N], Float y[N]) {
  static_assert(N % kMaxSimdWidth == 0);

  if constexpr (kUseAVX) {
    const __m256 kZero = {0, 0, 0, 0, 0, 0, 0, 0};
    for (int i = 0; i < N; i += 8) {
      auto v = _mm256_load_ps(&x[i]);
      v = _mm256_max_ps(v, kZero);
      _mm256_store_ps(&y[i], v);
    }

  } else if constexpr (kUseSSE) {
    const __m128 kZero = {0, 0, 0, 0};
    for (int i = 0; i < N; i += 4) {
      auto v = _mm_load_ps(&x[i]);
      v = _mm_max_ps(v, kZero);
      _mm_store_ps(&y[i], v);
    }

  } else {
    for (int i = 0; i < N; i++) {
      y[i] = std::max(x[i], 0.0f);
    }
  }
}

inline float sumSSE(__m128 x) {                             // a b c d
  __m128 y = _mm_shuffle_ps(x, x, _MM_SHUFFLE(2, 3, 0, 1)); // b a d c
  __m128 z = _mm_add_ps(x, y);                              // (a+b) (..) (c+d) (..)
  y = _mm_movehl_ps(y, z);                                  // (c+d) (..) (..)  (..)
  z = _mm_add_ss(y, z);                                     // (a+b+c+d) (..) (..) (..)
  return _mm_cvtss_f32(z);
}

inline float sumSSE3(__m128 x) { // a b c d
  __m128 y = _mm_movehdup_ps(x); // b b d d
  __m128 z = _mm_add_ps(x, y);   // (a+b) (..) (c+d) (..)
  y = _mm_movehl_ps(y, z);       // (c+d) (..) (..)  (..)
  z = _mm_add_ss(y, z);          // (a+b+c+d) (..) (..) (..)
  return _mm_cvtss_f32(z);
}

inline float sumAVX(__m256 x) {
  __m128 lo = _mm256_castps256_ps128(x);
  __m128 hi = _mm256_extractf128_ps(x, 1);
  return sumSSE3(_mm_add_ps(lo, hi));
}

template<int N>
inline Float dot(Float x[N], Float y[N]) {
  static_assert(N % (4 * kMaxSimdWidth) == 0);

  if constexpr (kUseAVX) {
    // Put four vectors on four registers
    auto z0 = _mm256_mul_ps(_mm256_load_ps(&x[0 * 8]), _mm256_load_ps(&y[0 * 8]));
    auto z1 = _mm256_mul_ps(_mm256_load_ps(&x[1 * 8]), _mm256_load_ps(&y[1 * 8]));
    auto z2 = _mm256_mul_ps(_mm256_load_ps(&x[2 * 8]), _mm256_load_ps(&y[2 * 8]));
    auto z3 = _mm256_mul_ps(_mm256_load_ps(&x[3 * 8]), _mm256_load_ps(&y[3 * 8]));
    for (int i = 4 * 8; i < N; i += 4 * 8) {
      if constexpr (kUseFMA) {
        z0 = _mm256_fmadd_ps(_mm256_load_ps(&x[i + 0 * 8]), _mm256_load_ps(&y[i + 0 * 8]), z0);
        z1 = _mm256_fmadd_ps(_mm256_load_ps(&x[i + 1 * 8]), _mm256_load_ps(&y[i + 1 * 8]), z1);
        z2 = _mm256_fmadd_ps(_mm256_load_ps(&x[i + 2 * 8]), _mm256_load_ps(&y[i + 2 * 8]), z2);
        z3 = _mm256_fmadd_ps(_mm256_load_ps(&x[i + 3 * 8]), _mm256_load_ps(&y[i + 3 * 8]), z3);
      } else {
        z0 = _mm256_add_ps(_mm256_mul_ps(_mm256_load_ps(&x[i + 0 * 8]), _mm256_load_ps(&y[i + 0 * 8])), z0);
        z1 = _mm256_add_ps(_mm256_mul_ps(_mm256_load_ps(&x[i + 1 * 8]), _mm256_load_ps(&y[i + 1 * 8])), z1);
        z2 = _mm256_add_ps(_mm256_mul_ps(_mm256_load_ps(&x[i + 2 * 8]), _mm256_load_ps(&y[i + 2 * 8])), z2);
        z3 = _mm256_add_ps(_mm256_mul_ps(_mm256_load_ps(&x[i + 3 * 8]), _mm256_load_ps(&y[i + 3 * 8])), z3);
      }
    }
    return sumAVX(_mm256_add_ps(_mm256_add_ps(z0, z1), _mm256_add_ps(z2, z3)));

  } else if constexpr (kUseSSE) {
    __m128 res = {0, 0, 0, 0};
    for (int i = 0; i < N; i += 4) {
      auto vx = _mm_load_ps(&x[i]);
      auto vy = _mm_load_ps(&y[i]);
      res = _mm_add_ps(res, _mm_mul_ps(vx, vy));
    }
    return sumSSE(res);

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
  static_assert(N % kMaxSimdWidth == 0);

  if constexpr (kUseAVX) {
    for (int i = 0; i < N; i += 8) {
      auto v = _mm256_load_ps(&x[i]);
      _mm256_store_ps(&y[i], v);
    }

  } else if constexpr (kUseSSE) {
    for (int i = 0; i < N; i += 4) {
      auto v = _mm_load_ps(&x[i]);
      _mm_store_ps(&y[i], v);
    }

  } else {
    for (int i = 0; i < N; i++) {
      y[i] = x[i];
    }
  }
}

template<int N>
inline void add(Float x[N], Float y[N], Float z[N]) {
  static_assert(N % kMaxSimdWidth == 0);

  if constexpr (kUseAVX) {
    for (int i = 0; i < N; i += 8) {
      auto vx = _mm256_load_ps(&x[i]);
      auto vy = _mm256_load_ps(&y[i]);
      auto vz = _mm256_add_ps(vx, vy);
      _mm256_store_ps(&z[i], vz);
    }

  } else if constexpr (kUseSSE) {
    for (int i = 0; i < N; i += 4) {
      auto vx = _mm_load_ps(&x[i]);
      auto vy = _mm_load_ps(&y[i]);
      auto vz = _mm_add_ps(vx, vy);
      _mm_store_ps(&z[i], vz);
    }

  } else {
    for (int i = 0; i < N; i++) {
      z[i] = x[i] + y[i];
    }
  }
}

template<int N>
inline void sub(Float x[N], Float y[N], Float z[N]) {
  static_assert(N % kMaxSimdWidth == 0);

  if constexpr (kUseAVX) {
    for (int i = 0; i < N; i += 8) {
      auto vx = _mm256_load_ps(&x[i]);
      auto vy = _mm256_load_ps(&y[i]);
      auto vz = _mm256_sub_ps(vx, vy);
      _mm256_store_ps(&z[i], vz);
    }

  } else if constexpr (kUseSSE) {
    for (int i = 0; i < N; i += 4) {
      auto vx = _mm_load_ps(&x[i]);
      auto vy = _mm_load_ps(&y[i]);
      auto vz = _mm_sub_ps(vx, vy);
      _mm_store_ps(&z[i], vz);
    }

  } else {
    for (int i = 0; i < N; i++) {
      z[i] = x[i] - y[i];
    }
  }
}

template<int N1, int N2>
struct Linear {
  static_assert(N1 % kMaxSimdWidth == 0);

  alignas(kMaxFloatVectorSize) Float weight[N2][N1] = {};
  alignas(kMaxFloatVectorSize) Float bias[N2] = {};

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

  float forwardOne(int i, float x[N1]) {
    return dot<N1>(weight[i], x) + bias[i];
  }
};

template<int N1, int N2>
struct InputLayer {
  static_assert(N2 % kMaxSimdWidth == 0);

  alignas(kMaxFloatVectorSize) Float weight[N1][N2] = {}; // Contiguous in output dimention
  alignas(kMaxFloatVectorSize) Float bias[N2] = {};

  void load(std::istream& istr) {
    // NOTE: pytorch's embedding weight is already transposed
    istr.read(reinterpret_cast<char*>(weight[0]), N1 * N2 * kFloatSize);
    ASSERT(istr.gcount() == N1 * N2 * kFloatSize);
    istr.read(reinterpret_cast<char*>(bias), N2 * kFloatSize);
    ASSERT(istr.gcount() == N2 * kFloatSize);
  }
};

}; // namespace nn
