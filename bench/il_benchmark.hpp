#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <ranges>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

// Force inlining on the hot path (timer reads, sink). Standard `inline` is only
// a hint; this attribute actually compels inlining at -O1+.
#if defined(__GNUC__) || defined(__clang__)
#define BENCH_ALWAYS_INLINE __attribute__((always_inline)) inline
#elif defined(_MSC_VER)
#define BENCH_ALWAYS_INLINE __forceinline
#else
#define BENCH_ALWAYS_INLINE inline
#endif

namespace bench {

// ---------------------------------------------------------------------------
// Arch-specific timer primitives. All emit a leading barrier (prevent prior
// loads from being hoisted after the read) and return the counter value.
// `timer_stop` additionally emits a trailing barrier so later code cannot be
// hoisted up before the read.
// ---------------------------------------------------------------------------

#if defined(__aarch64__)

BENCH_ALWAYS_INLINE uint64_t timer_start() {
  uint64_t v;
  asm volatile("isb \n\t"
               "mrs %0, cntvct_el0"
               : "=r"(v));
  return v;
}

BENCH_ALWAYS_INLINE uint64_t timer_stop() {
  uint64_t v;
  asm volatile("isb \n\t"
               "mrs %0, cntvct_el0 \n\t"
               "isb"
               : "=r"(v));
  return v;
}

inline uint64_t timer_freq() {
  uint64_t f;
  asm volatile("mrs %0, cntfrq_el0" : "=r"(f));
  return f;
}

#elif defined(__x86_64__) || defined(__amd64__)

BENCH_ALWAYS_INLINE uint64_t timer_start() {
  unsigned hi, lo;
  // lfence = leading barrier (≈ isb); rdtsc returns EDX:EAX.
  asm volatile("lfence\n\t"
               "rdtsc"
               : "=a"(lo), "=d"(hi)
               : /* no inputs */
               : "memory");
  return (static_cast<uint64_t>(hi) << 32) | lo;
}

BENCH_ALWAYS_INLINE uint64_t timer_stop() {
  unsigned hi, lo;
  // rdtscp serializes before reading (leading barrier); lfence after prevents
  // later loads from hoisting up before the read (trailing barrier). clobbers
  // RCX (the aux MSR is read into it).
  asm volatile("rdtscp" : "=a"(lo), "=d"(hi) : : "rcx", "memory");
  asm volatile("lfence" ::: "memory");
  return (static_cast<uint64_t>(hi) << 32) | lo;
}

inline uint64_t timer_freq() {
  // x86 has no register exposing the TSC rate; calibrate once against
  // steady_clock. Assumes invariant TSC (all modern Intel/AMD).
  static const uint64_t freq = [] {
    auto t0 = std::chrono::steady_clock::now();
    auto c0 = timer_start();
    while (std::chrono::steady_clock::now() - t0 <
           std::chrono::milliseconds(10)) {
    }
    auto c1 = timer_start();
    auto t1 = std::chrono::steady_clock::now();
    auto ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    return static_cast<uint64_t>(c1 - c0) * 1'000'000'000ULL /
           static_cast<uint64_t>(ns);
  }();
  return freq;
}

#else
#error                                                                         \
    "Unsupported architecture: add a timer_start/timer_stop implementation for this platform."
#endif

// ---------------------------------------------------------------------------
// Output sink: forces the compiler to materialize `v` in a register and treat
// it as read+written, defeating constant folding and dead-code elimination.
// The empty asm body emits no instruction; the "memory" clobber additionally
// blocks hoisting of surrounding loads/stores.
// ---------------------------------------------------------------------------
template <typename T> BENCH_ALWAYS_INLINE void keep(T &v) {
  asm volatile("" : "+r"(const_cast<T &>(v)) : : "memory");
}

// ---------------------------------------------------------------------------
// Benchmark a single invocation of f(args...). Perfect-forwards args, sinks the
// result. Returns {result, elapsed ticks}.
//
// Note: unreliable for sub-tick work — per-sample mrs/isb overhead dominates.
// Use benchmark_N for fast functions.
// ---------------------------------------------------------------------------
template <typename F, typename... Args>
std::pair<std::invoke_result_t<F &, Args...>, uint64_t>
il_benchmark(F &&f, Args &&...args) {
  uint64_t start_val = timer_start();

  auto res = f(std::forward<Args>(args)...);
  keep(res);

  uint64_t stop_val = timer_stop();

  return std::make_pair(res, stop_val - start_val);
}

// ---------------------------------------------------------------------------
// Aggregate N invocations under a single timer window. The timer overhead
// amortizes to ~0 per iteration and the work accumulates above one tick.
// Result of each iteration is sunk with keep() so the loop body cannot be
// DCE'd. Args are captured once by reference (std::forward_as_tuple) outside
// the timed window; they must outlive the timed calls.
//
// Requirement: f must return non-void (the sink needs something to pin).
// ---------------------------------------------------------------------------
template <std::size_t N, typename F, typename... Args>
uint64_t benchmark_N(F &&f, Args &&...args) {
  auto run = [f = std::forward<F>(f),
              tup = std::forward_as_tuple(
                  std::forward<Args>(args)...)]() mutable -> decltype(auto) {
    return std::apply(f, tup);
  };
  static_assert(!std::is_void_v<decltype(run())>,
                "benchmark_N requires f to return non-void");

  uint64_t start = timer_start();
  for ([[maybe_unused]] auto i : std::views::iota(0uz) | std::views::take(N)) {
    auto res = run();
    keep(res);
  }
  uint64_t stop = timer_stop();
  return stop - start;
}

// ---------------------------------------------------------------------------
// Collect N per-iteration tick samples by reusing il_benchmark. reserve(N)
// upfront; push_back runs strictly after the stop timer read so vector
// bookkeeping never enters the timed window.
//
// Note: not adequate for sub-tick f — per-sample timer overhead swamps the
// signal. Use benchmark_N instead.
// ---------------------------------------------------------------------------
template <std::size_t N, typename F, typename... Args>
std::vector<uint64_t> benchmark_samples(F &&f, Args &&...args) {
  std::vector<uint64_t> ticks;
  ticks.reserve(N);
  auto run = [f = std::forward<F>(f),
              tup = std::forward_as_tuple(
                  std::forward<Args>(args)...)]() mutable -> decltype(auto) {
    return std::apply(f, tup);
  };
  static_assert(!std::is_void_v<decltype(run())>,
                "benchmark_samples requires f to return non-void");
  for ([[maybe_unused]] auto i : std::views::iota(0uz) | std::views::take(N)) {
    auto [_, t] = il_benchmark(run);
    ticks.push_back(t);
  }
  return ticks;
}

// ---------------------------------------------------------------------------
// Calculate p<N> (nearest-rank, 1 <= N <= 100) of runtimes collected through
// benchmark_samples. Sorts vec in place. Returns 0 on insufficient samples
// (with an error on stderr).
// ---------------------------------------------------------------------------
template <std::size_t N> auto p(std::vector<uint64_t> &vec) {
  static_assert(N >= 1 && N <= 100, "p<N> requires 1 <= N <= 100");
  if (vec.size() < N) {
    std::cerr << "Cannot calculate p<" << N
              << ">. Increase number of samples. Current "
                 "number of samples: "
              << vec.size() << std::endl;
    return uint64_t{0};
  }
  std::sort(vec.begin(), vec.end());

  auto index = static_cast<uint64_t>(std::ceil(vec.size() * N / 100.0)) - 1;
  return vec.at(index);
}

// ---------------------------------------------------------------------------
// Portable wallclock timing using std::chrono::steady_clock. Independent of
// the CPU virtual counter. Sinks the result. For sub-µs work, wrap an N-rep
// loop and divide by N to amortize now() overhead.
// ---------------------------------------------------------------------------
template <typename F, typename... Args>
auto wallclock_spend(F &&f, Args &&...args) {
  auto begin{std::chrono::steady_clock::now()};
  auto res = f(std::forward<Args>(args)...);
  keep(res);
  auto end{std::chrono::steady_clock::now()};
  return std::chrono::duration<double>(end - begin);
}

} // namespace bench
