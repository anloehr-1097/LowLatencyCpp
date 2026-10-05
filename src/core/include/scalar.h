#pragma once
#include <cstddef>
namespace simd {

// float types and ops
using f32v = float;
inline constexpr std::size_t width = 1;
inline f32v load(const float *p) { return *p; }
inline void store(float *p, f32v v) { *p = v; }
inline f32v add(f32v a, f32v b) { return a + b; }

} // namespace simd
