#pragma once

#include <chrono>
#include <cstdint>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace bench {

/*
 * Output sink:
 * ensure the compiler doesn to calculate v at compile time and enforce compiler
 * memory barrier. Serving as an output sink, indicating to compiler that this
 * value is read, write, so compiler must materialize val in a register. Used to
 * force computation in the benchmark function and prevent Dead code
 * elimination.
 */
template <typename T> void keep(T &v) {
  asm volatile("" : "+r"(const_cast<T &>(v)) : : "memory");
}

/*
 * Benchmark single invocation of function with args.
 * Note: on sub CPU timer tick level, the results from this call are unreliable.
 */
template <typename F, typename... Args>
std::pair<std::invoke_result_t<F &, Args...>, uint64_t>
il_benchmark(F &&f, Args &&...args) {
  // forward args to f, benchmark
  //
  uint64_t start_val, stop_val;

  // memory barrier instruction, then record CPU timer
  asm volatile("isb \n\t"
               "mrs %0, cntvct_el0"
               : "=r"(start_val));

  auto res = f(std::forward<Args>(args)...);

  keep(res);
  asm volatile("isb \n\t"
               "mrs %0, cntvct_el0 \n\t"
               "isb"
               : "=r"(stop_val));

  return std::make_pair(res, stop_val - start_val);
}

/*
 * Benchmark function N times, returning aggregate time taken.
 * This is an alternative to `benchmark_samples` and `il_benchmark` for
 * functions running sub CPU time tick long.
 *
 */
template <std::size_t N, typename F, typename... Args>
uint64_t benchmark_N(F &&f, Args &&...args) {
  auto run = [f = std::forward<F>(f),
              tup = std::make_tuple(std::forward<Args>(args)...)]() mutable
      -> decltype(auto) { return std::apply(f, tup); };
  static_assert(!std::is_void_v<decltype(run())>,
                "benchmark_N requires f to return non-void");

  uint64_t start, stop;
  asm volatile("isb \n\t"
               "mrs %0, cntvct_el0"
               : "=r"(start));
  for ([[maybe_unused]] auto i : std::views::iota(0uz) | std::views::take(N)) {
    auto res = run();
    keep(res);
  }
  asm volatile("isb \n\t"
               "mrs %0, cntvct_el0 \n\t"
               "isb"
               : "=r"(stop));
  return stop - start;
}

/*
 * Aggregate N timed runs into vector. Allows statistical eval.
 * Note: this benchmarking function is *not* adequate to benchmark sub CPU time
 * stamp tick frequency functions. Use `benchmark_N` instead.
 */
template <std::size_t N, typename F, typename... Args>
std::vector<uint64_t> benchmark_samples(F &&f, Args &&...args) {
  std::vector<uint64_t> ticks;
  ticks.reserve(N);
  auto run = [f = std::forward<F>(f),
              tup = std::make_tuple(std::forward<Args>(args)...)]() mutable
      -> decltype(auto) { return std::apply(f, tup); };
  static_assert(!std::is_void_v<decltype(run())>,
                "benchmark_samples requires f to return non-void");
  for ([[maybe_unused]] auto i : std::views::iota(0uz) | std::views::take(N)) {
    auto [_, t] = il_benchmark(run);
    ticks.push_back(t);
  }
  return ticks;
}

template <typename F, typename... Args>
auto wallclock_spend(F &&f, Args &&...args) {
  auto begin{std::chrono::steady_clock::now()};
  auto res = f(std::forward<Args>(args)...);
  keep(res);
  auto end{std::chrono::steady_clock::now()};
  return std::chrono::duration<double>(end - begin);
}
} // namespace bench
