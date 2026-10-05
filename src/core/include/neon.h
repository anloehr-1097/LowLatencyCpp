#pragma once
#include <arm_neon.h>
#include <cstddef>
namespace simd {

// float types and ops
using f32v = float32x4_t;
inline constexpr std::size_t width = 4;
inline f32v load(const float *p) { return vld1q_f32(p); }
inline void store(float *p, f32v v) { vst1q_f32(p, v); }
inline f32v add(f32v a, f32v b) { return vaddq_f32(a, b); }

} // namespace simd
