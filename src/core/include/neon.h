#pragma once
#include <arm_neon.h>
#include <cstddef>
#include <cstdint>
namespace simd {

template <typename T> struct vec_traits;

// mapping c++ float -> simd type
// used in templated implementations with simd ops
template <> struct vec_traits<float> {
  using vec_t = float32x4_t;
  static constexpr size_t width = 4;
};

template <> struct vec_traits<int32_t> {
  using vec_t = int32x4_t;
  static constexpr size_t width = 4;
};

// float types and ops
using f32v = float32x4_t;
inline constexpr std::size_t width = 4;
inline f32v load(const float *p) { return vld1q_f32(p); }
inline void store(float *p, f32v v) { vst1q_f32(p, v); }
inline f32v add(f32v a, f32v b) { return vaddq_f32(a, b); }
inline f32v sub(f32v a, f32v b) { return vsubq_f32(a, b); }
inline f32v mul(f32v a, f32v b) { return vmulq_f32(a, b); }
// true iff every lane compares equal (NaN != NaN, +0 == -0, like scalar ==)
inline bool all_eq(f32v a, f32v b) {
  return vminvq_u32(vceqq_f32(a, b)) == 0xFFFFFFFFu;
}

// int types and ops
using int32v = int32x4_t;
inline int32v load(const int32_t *p) { return vld1q_s32(p); }
inline void store(int32_t *p, int32v v) { vst1q_s32(p, v); }
inline int32v add(int32v a, int32v b) { return vaddq_s32(a, b); }
inline int32v sub(int32v a, int32v b) { return vsubq_s32(a, b); }
inline int32v mul(int32v a, int32v b) { return vmulq_s32(a, b); }
inline bool all_eq(int32v a, int32v b) {
  return vminvq_u32(vceqq_s32(a, b)) == 0xFFFFFFFFu;
}

} // namespace simd
