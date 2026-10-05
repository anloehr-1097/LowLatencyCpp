#pragma once
#include <cstddef>
#include <cstdint>
#include <immintrin.h>
namespace simd {

template <typename T> struct vec_traits;
template <> struct vec_traits<float> {
  using vec_t = __m256;
  static constexpr size_t width = 8;
};

template <> struct vec_traits<int32_t> {
  using vec_t = __m256i;
  static constexpr size_t width = 8;
};

// float types and ops
using f32v = __m256;
inline f32v load(const float *p) { return _mm256_loadu_ps(p); }
inline void store(float *p, f32v v) { _mm256_storeu_ps(p, v); }
inline f32v add(f32v a, f32v b) { return _mm256_add_ps(a, b); }

// int types and ops
using int32v = __m256i;
inline int32v load(const int32_t *p) {
  return _mm256_loadu_si256(reinterpret_cast<const __m256i *>(p));
}
inline void store(int32_t *p, int32v v) {
  _mm256_storeu_si256(reinterpret_cast<__m256i *>(p), v);
}
inline int32v add(int32v a, int32v b) { return _mm256_add_epi32(a, b); }

// TODO(al) unsigned int types and ops
using uint32v = __m256_u;

} // namespace simd
