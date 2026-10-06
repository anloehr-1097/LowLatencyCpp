#pragma once
#include <cstddef>
#include <cstdint>
namespace simd {

template <typename T> struct vec_traits;

// mapping c++ float -> simd type
// used in templated implementations with simd ops
template <> struct vec_traits<float> {
  using vec_t = float;
  static constexpr size_t width = 1;
};

template <> struct vec_traits<int32_t> {
  using vec_t = int32_t;
  static constexpr size_t width = 1;
};

// float types and ops
using f32v = float;
inline constexpr std::size_t width = 1;
inline f32v load(const float *p) { return *p; }
inline void store(float *p, f32v v) { *p = v; }
inline f32v add(f32v a, f32v b) { return a + b; }
inline f32v sub(f32v a, f32v b) { return a - b; }
inline f32v mul(f32v a, f32v b) { return a * b; }
inline bool all_eq(f32v a, f32v b) { return a == b; }

// int types and ops
using int32v = int32_t;
inline int32v load(const int32_t *p) { return *p; }
inline void store(int32_t *p, int32v v) { *p = v; }
inline int32v add(int32v a, int32v b) { return a + b; }
inline int32v sub(int32v a, int32v b) { return a - b; }
inline int32v mul(int32v a, int32v b) { return a * b; }
inline bool all_eq(int32v a, int32v b) { return a == b; }

} // namespace simd
