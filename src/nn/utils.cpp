#include "utils.hpp"
#include <config.hpp>
#include <immintrin.h>

namespace nn {

template<int N>
void relu(const float x[N], float y[N]) {
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

float sumSSE(__m128 x) {                             // a b c d
  __m128 y = _mm_shuffle_ps(x, x, _MM_SHUFFLE(2, 3, 0, 1)); // b a d c
  __m128 z = _mm_add_ps(x, y);                              // (a+b) (..) (c+d) (..)
  y = _mm_movehl_ps(y, z);                                  // (c+d) (..) (..)  (..)
  z = _mm_add_ss(y, z);                                     // (a+b+c+d) (..) (..) (..)
  return _mm_cvtss_f32(z);
}

float sumSSE3(__m128 x) { // a b c d
  __m128 y = _mm_movehdup_ps(x); // b b d d
  __m128 z = _mm_add_ps(x, y);   // (a+b) (..) (c+d) (..)
  y = _mm_movehl_ps(y, z);       // (c+d) (..) (..)  (..)
  z = _mm_add_ss(y, z);          // (a+b+c+d) (..) (..) (..)
  return _mm_cvtss_f32(z);
}

float sumAVX(__m256 x) {
  __m128 lo = _mm256_castps256_ps128(x);
  __m128 hi = _mm256_extractf128_ps(x, 1);
  return sumSSE3(_mm_add_ps(lo, hi));
}

template<int N>
float dot(const float x[N], const float y[N]) {
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
    float res = 0;
    for (int i = 0; i < N; i++) {
      res += x[i] * y[i];
    }
    return res;
  }
}

template<int N>
void dot4(const float a[][N], const float x[N], const float b[], float y[]) {
  static_assert(N % kMaxSimdWidth == 0);

  if constexpr (kUseAVX) {
    auto x0 = _mm256_load_ps(&x[0]);
    auto z0 = _mm256_mul_ps(_mm256_load_ps(&a[0][0]), x0);
    auto z1 = _mm256_mul_ps(_mm256_load_ps(&a[1][0]), x0);
    auto z2 = _mm256_mul_ps(_mm256_load_ps(&a[2][0]), x0);
    auto z3 = _mm256_mul_ps(_mm256_load_ps(&a[3][0]), x0);
    for (int i = 8; i < N; i += 8) {
      auto xv = _mm256_load_ps(&x[i]);
      if constexpr (kUseFMA) {
        z0 = _mm256_fmadd_ps(_mm256_load_ps(&a[0][i]), xv, z0);
        z1 = _mm256_fmadd_ps(_mm256_load_ps(&a[1][i]), xv, z1);
        z2 = _mm256_fmadd_ps(_mm256_load_ps(&a[2][i]), xv, z2);
        z3 = _mm256_fmadd_ps(_mm256_load_ps(&a[3][i]), xv, z3);
      } else {
        z0 = _mm256_add_ps(_mm256_mul_ps(_mm256_load_ps(&a[0][i]), xv), z0);
        z1 = _mm256_add_ps(_mm256_mul_ps(_mm256_load_ps(&a[1][i]), xv), z1);
        z2 = _mm256_add_ps(_mm256_mul_ps(_mm256_load_ps(&a[2][i]), xv), z2);
        z3 = _mm256_add_ps(_mm256_mul_ps(_mm256_load_ps(&a[3][i]), xv), z3);
      }
    }

    auto haddx4 = [](__m256 x0, __m256 x1, __m256 x2, __m256 x3) -> __m128 {
      auto x = _mm256_hadd_ps(_mm256_hadd_ps(x0, x1), _mm256_hadd_ps(x2, x3));
      auto lo = _mm256_extractf128_ps(x, 0);
      auto hi = _mm256_extractf128_ps(x, 1);
      return _mm_add_ps(lo, hi);
    };
    _mm_store_ps(y, _mm_add_ps(_mm_load_ps(b), haddx4(z0, z1, z2, z3)));

  } else {
    for (int i = 0; i < 4; i++) {
      y[i] = dot<N>(a[i], x) + b[i];
    }
  }
}

template<int N>
void copy(const float x[N], float y[N]) {
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
void add(const float x[N], const float y[N], float z[N]) {
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
void sub(const float x[N], const float y[N], float z[N]) {
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
void affine(const float A[N2][N1], const float x[N1], const float b[N2], float y[N2]) {
  if constexpr (N2 % 4 == 0) {
    for (int i = 0; i < N2; i += 4) {
      dot4<N1>(&A[i], x, &b[i], &y[i]);
    }

  } else {
    for (int i = 0; i < N2; i++) {
      y[i] = dot<N1>(A[i], x) + b[i];
    }
  }
}

// Explicit instantiation
template void relu<256>(const float x[256], float y[256]);
template void relu<32>(const float x[32], float y[32]);

template void copy<128>(const float x[128], float y[128]);
template void add<128>(const float x[128], const float y[128], float z[128]);
template void sub<128>(const float x[128], const float y[128], float z[128]);

template void affine<256, 32>(const float A[32][256], const float x[256], const float b[32], float y[32]);
template void affine< 32, 32>(const float A[32][ 32], const float x[ 32], const float b[32], float y[32]);
template void affine< 32,  1>(const float A[ 1][ 32], const float x[ 32], const float b[ 1], float y[ 1]);

}; // namespace nn
