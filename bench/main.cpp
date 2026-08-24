#include <algorithm>
#include <iostream>
#include <numeric>

#include "il_benchmark.hpp"

int main() {

  auto func = [](int a, int b) { return a * b; };

  auto [res, time] = bench::il_benchmark(func, 3, 3);
  bench::keep(res); // here, due to printing value, not required. In general, it
                    // is though to prevent Dead code elimination.

  constexpr auto total_runs = 1000;
  uint64_t total = bench::benchmark_N<total_runs>(func, 3, 3);

  auto avg_benchmark_N =
      static_cast<float>(total) / static_cast<float>(total_runs);
  auto samples = bench::benchmark_samples<total_runs>(func, 3, 3);

  auto min_it = std::ranges::min(samples);
  auto avg = std::accumulate(samples.begin(), samples.end(), uint64_t{0}) /
             samples.size();

  auto wc_res = bench::wallclock_spend(func, 3, 3);

  std::cout << "Result: " << res << "\ttime: " << time << "\n"
            << "total (1000 reps): " << total << "\n"
            << "avg from total: " << avg_benchmark_N << "\n"
            << "samples min: " << min_it << " avg: " << avg << "\n"
            << "Wallclack time: " << wc_res << "\n";
  return 0;
}
