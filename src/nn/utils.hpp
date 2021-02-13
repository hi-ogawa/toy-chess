#pragma once

#include "../misc.hpp"

namespace nn {

inline constexpr size_t kMaxSimdWidth = 8;
inline constexpr size_t kMaxFloatVectorSize = sizeof(float) * kMaxSimdWidth;

template<int N>
void relu(const float x[N], float y[N]);

template<int N>
void copy(const float x[N], float y[N]);

template<int N>
void add(const float x[N], const float y[N], float z[N]);

template<int N>
void sub(const float x[N], const float y[N], float z[N]);

template<int N1, int N2>
void affine(const float A[N2][N1], const float x[N1], const float b[N2], float y[N2]);

template<int N1, int N2>
struct Linear {
  static_assert(N1 % kMaxSimdWidth == 0);

  alignas(kMaxFloatVectorSize) float weight[N2][N1] = {};
  alignas(kMaxFloatVectorSize) float bias[N2] = {};

  void load(std::istream& istr) {
    istr.read(reinterpret_cast<char*>(weight[0]), N1 * N2 * sizeof(float));
    ASSERT(istr.gcount() == N1 * N2 * sizeof(float));
    istr.read(reinterpret_cast<char*>(bias), N2 * sizeof(float));
    ASSERT(istr.gcount() == N2 * sizeof(float));
  }

  void forward(float x[N1], float y[N2]) {
    affine<N1, N2>(weight, x, bias, y);
  }
};

template<int N1, int N2>
struct InputLayer {
  static_assert(N2 % kMaxSimdWidth == 0);

  alignas(kMaxFloatVectorSize) float weight[N1][N2] = {}; // Contiguous in output dimention
  alignas(kMaxFloatVectorSize) float bias[N2] = {};

  void load(std::istream& istr) {
    // NOTE: pytorch's embedding weight is already transposed
    istr.read(reinterpret_cast<char*>(weight[0]), N1 * N2 * sizeof(float));
    ASSERT(istr.gcount() == N1 * N2 * sizeof(float));
    istr.read(reinterpret_cast<char*>(bias), N2 * sizeof(float));
    ASSERT(istr.gcount() == N2 * sizeof(float));
  }
};

}; // namespace nn
