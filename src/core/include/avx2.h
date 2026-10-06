#pragma once
#include <cstddef>
#include <cstdint>
#include <immintrin.h>
namespace simd {

template <typename T> struct vec_traits;

// mapping c++ float -> simd type
// used in templated implementations with simd ops
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
inline f32v sub(f32v a, f32v b) { return _mm256_sub_ps(a, b); }
inline f32v mul(f32v a, f32v b) { return _mm256_mul_ps(a, b); }
// true iff every lane compares equal (NaN != NaN, +0 == -0, like scalar ==)
inline bool all_eq(f32v a, f32v b) {
  return _mm256_movemask_ps(_mm256_cmp_ps(a, b, _CMP_EQ_OQ)) == 0xFF;
}

// int types and ops
using int32v = __m256i;
inline int32v load(const int32_t *p) {
  return _mm256_loadu_si256(reinterpret_cast<const __m256i *>(p));
}
inline void store(int32_t *p, int32v v) {
  _mm256_storeu_si256(reinterpret_cast<__m256i *>(p), v);
}
inline int32v add(int32v a, int32v b) { return _mm256_add_epi32(a, b); }
inline int32v sub(int32v a, int32v b) { return _mm256_sub_epi32(a, b); }
inline int32v mul(int32v a, int32v b) { return _mm256_mullo_epi32(a, b); }
inline bool all_eq(int32v a, int32v b) {
  return _mm256_movemask_epi8(_mm256_cmpeq_epi32(a, b)) == -1;
}

// TODO(al) unsigned int types and ops
using uint32v = __m256_u;

} // namespace simd
