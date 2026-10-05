#pragma once
#include <immintrin.h>
namespace simd {

// float types and ops
using f32v = __m256;
inline constexpr std::size_t width = 8;
inline f32v load(const float *p) { return _mm256_load_ps(p); }
inline void store(float *p, f32v v) { _mm256_storeu_ps(p, v); }
inline f32v add(f32v a, f32v b) { return _mm256_add_ps(a, b); }

} // namespace simd
