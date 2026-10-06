#ifndef LINALG_H
#define LINALG_H

#include "simd.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>
#include <utility>

/*
 * Vector Datastruct specifically for 3D and 6D vectors
 * supported operations: add, sub, mult, div, dot prod
 *
 */

template <typename T, std::size_t N> struct VectNd {
  T data[N];
  constexpr VectNd() : data{} {}

  // Note: num args must match N
  template <typename... Arg>
  constexpr VectNd(Arg &&...args)
      : data{static_cast<T>(std::forward<Arg>(args))...} {}

  constexpr VectNd(std::initializer_list<T> ls) : data{} {
    std::copy_n(ls.begin(), std::min(ls.size(), N), data);
  }

  constexpr bool as_divisor() const {
    return std::find_if(data, data + N, [](T val) { return val == 0; }) !=
           data + N;
  }
};

// ---------------------------------------------------------------------------
// Element-wise arithmetic: add, subtract, multiply, divide
// ---------------------------------------------------------------------------

template <typename T, std::size_t N>
constexpr VectNd<T, N> operator+(const VectNd<T, N> &lhs,
                                 const VectNd<T, N> &rhs) {
  VectNd<T, N> out;
  std::size_t i = 0;
  for (; i + simd::vec_traits<T>::width <= N; i += simd::vec_traits<T>::width) {
    simd::store(out.data + i,
                simd::add(simd::load(lhs.data + i), simd::load(rhs.data + i)));
  }
  for (; i < N; ++i) {
    out.data[i] = lhs.data[i] + rhs.data[i];
  }
  return out;
}

template <typename T, std::size_t N>
constexpr VectNd<T, N> operator-(const VectNd<T, N> &lhs,
                                 const VectNd<T, N> &rhs) {
  VectNd<T, N> out;
  std::size_t i = 0;
  for (; i + simd::vec_traits<T>::width <= N; i += simd::vec_traits<T>::width) {
    simd::store(out.data + i,
                simd::sub(simd::load(lhs.data + i), simd::load(rhs.data + i)));
  }
  for (; i < N; ++i) {
    out.data[i] = lhs.data[i] - rhs.data[i];
  }
  return out;
}

template <typename T, std::size_t N>
constexpr VectNd<T, N> operator*(const VectNd<T, N> &lhs,
                                 const VectNd<T, N> &rhs) {
  VectNd<T, N> out;
  std::size_t i = 0;
  for (; i + simd::vec_traits<T>::width <= N; i += simd::vec_traits<T>::width) {
    simd::store(out.data + i,
                simd::mul(simd::load(lhs.data + i), simd::load(rhs.data + i)));
  }
  for (; i < N; ++i) {
    out.data[i] = lhs.data[i] * rhs.data[i];
  }
  return out;
}

template <typename T, std::size_t N>
constexpr VectNd<T, N> operator/(const VectNd<T, N> &lhs,
                                 const VectNd<T, N> &rhs) {
  VectNd<T, N> out;
  if (!rhs.as_divisor()) {
    return out;
  }
  for (std::size_t i = 0; i < N; ++i)
    out.data[i] = lhs.data[i] / rhs.data[i];
  return out;
}

// ---------------------------------------------------------------------------
// Comparison
// ---------------------------------------------------------------------------

template <typename T, std::size_t N>
constexpr bool operator==(const VectNd<T, N> &lhs, const VectNd<T, N> &rhs) {
  std::size_t i = 0;
  for (; i + simd::vec_traits<T>::width <= N; i += simd::vec_traits<T>::width) {
    if (!simd::all_eq(simd::load(lhs.data + i), simd::load(rhs.data + i))) {
      return false;
    }
  }
  for (; i < N; ++i) {
    if (rhs.data[i] != lhs.data[i]) {
      return false;
    }
  }
  return true;
}
#endif // LINALG_H
