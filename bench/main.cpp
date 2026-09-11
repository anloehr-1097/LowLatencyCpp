#include <algorithm>
#include <chrono>
#include <iostream>
#include <numeric>
#include <ranges>

#include "il_benchmark.hpp"

int main() {

  // 1. Simple Function single invocation benchmarked
  auto func = [](int a, int b) { return a * b; };
  volatile auto a = 3;
  volatile auto b = 3;
  auto [res_simple, time] = bench::il_benchmark(func, a, b);
  // here, due to printing value later, not required. In general, it is though
  // to prevent Dead code elimination.
  bench::keep(res_simple);

  // 2. N invocations under single timer invocation
  constexpr auto total_runs = 1000;
  uint64_t total = bench::benchmark_N<total_runs>(func, a, b);
  auto avg_benchmark_N =
      static_cast<float>(total) / static_cast<float>(total_runs);

  // 3. N invocations, one timer invocation per sample, keeping all N timing
  // results
  auto samples = bench::benchmark_samples<total_runs>(func, a, b);
  auto min_it = std::ranges::min(samples);
  auto p99 = bench::p<99>(samples);
  auto [avg, stddev] = bench::standard_dev(samples);

  // 4. N invocations with M warmup iterations
  constexpr auto warmup_iters = 10UL;
  auto samples_wu =
      bench::benchmark_samples<total_runs, warmup_iters>(func, a, b);
  auto min_it_wu = std::ranges::min(samples_wu);
  auto avg_wu =
      std::accumulate(samples_wu.begin(), samples_wu.end(), uint64_t{0}) /
      samples_wu.size();
  auto p99_wu = bench::p<99>(samples_wu);

  // 5. Wallclock time for N invocations
  auto wc_func_N = [total_runs = total_runs, func = func, &a = a, &b = b]() {
    for ([[maybe_unused]] auto i :
         std::views::iota(0uz) | std::views::take(total_runs)) {
      auto res = func(a, b);
      bench::keep(res); // sink each iteration — prevent DCE of the mul
    }
    return 0;
  };
  auto wc_res = bench::wallclock_spend(wc_func_N);
  auto wc_ns_per_rep =
      std::chrono::duration<double, std::nano>(wc_res).count() / total_runs;

  // Note: these prints rely on hte fact that the CPU TSC runs at 1Ghz interval,
  // i.e. one tick = 1ns for other architectures, make sure to adjust this to
  // the specific arch
  std::cout << "Result: " << res_simple << "\ttime: " << time << "\n"
            << "total (1000 reps): " << total << "\n"
            << "avg from total (ns/rep): " << avg_benchmark_N << "\n"
            << "samples min: " << min_it << " avg: " << avg << "\n"
            << "samples p99: " << p99 << "\n"
            << "samples min warmup: " << min_it_wu << " avg warmup: " << avg_wu
            << "\n"
            << "samples p99 warmup: " << p99_wu << "\n"
            << "samples stddev: " << stddev << "\n"
            << "Wallclock ns/rep: " << wc_ns_per_rep << "\n";
  return 0;
}
