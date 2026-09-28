#include "il_benchmark.hpp"
#include "include/ArenaAllocator.h"
#include "include/Particle.h"
#include "include/SPSCQueue.h"
#include "include/fixed_point.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <future>
#include <iostream>
#include <numeric>
#include <random>
#include <ranges>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace {

// Fixed seed: AoS and SoA built with the same seed contain identical values
// per particle index (see draw-order note below).
constexpr std::uint64_t kGenSeed = 42;

std::vector<Particle> make_particles_aos(std::size_t n, std::uint64_t seed) {
  std::mt19937_64 rng(seed);
  std::uniform_real_distribution<double> pos{0.0, 100.0};
  std::uniform_real_distribution<double> vel{-5.0, 5.0};
  std::uniform_real_distribution<double> mass{0.1, 10.0};
  std::uniform_real_distribution<double> circ{0.0, 300.0};

  std::vector<Particle> aos;
  aos.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    // Braced-init lists evaluate left-to-right, so the draw order matches
    // make_particles_soa exactly: same seed => same particle i in both
    // layouts.
    aos.push_back(Particle{pos(rng), pos(rng), pos(rng), vel(rng), vel(rng),
                           vel(rng), mass(rng), circ(rng)});
  }
  return aos;
}

SoaParticle make_particles_soa(std::size_t n, std::uint64_t seed) {
  std::mt19937_64 rng(seed);
  std::uniform_real_distribution<double> pos{0.0, 100.0};
  std::uniform_real_distribution<double> vel{-5.0, 5.0};
  std::uniform_real_distribution<double> mass{0.1, 10.0};
  std::uniform_real_distribution<double> circ{0.0, 300.0};

  SoaParticle soa;
  soa.x.reserve(n);
  soa.y.reserve(n);
  soa.z.reserve(n);
  soa.vx.reserve(n);
  soa.vy.reserve(n);
  soa.vz.reserve(n);
  soa.mass.reserve(n);
  soa.circumference.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    soa.x.push_back(pos(rng));
    soa.y.push_back(pos(rng));
    soa.z.push_back(pos(rng));
    soa.vx.push_back(vel(rng));
    soa.vy.push_back(vel(rng));
    soa.vz.push_back(vel(rng));
    soa.mass.push_back(mass(rng));
    soa.circumference.push_back(circ(rng));
  }
  return soa;
}

// Content-preserving AoS -> SoA transform: same particles, different memory
// layout. Lets the AoS and SoA code paths run on byte-identical data.
SoaParticle make_particles_soa_from_aos(const std::vector<Particle> &aos) {
  const std::size_t n = aos.size();
  SoaParticle soa;
  soa.x.resize(n);
  soa.y.resize(n);
  soa.z.resize(n);
  soa.vx.resize(n);
  soa.vy.resize(n);
  soa.vz.resize(n);
  soa.mass.resize(n);
  soa.circumference.resize(n);

  for (std::size_t i = 0; i < n; ++i) {
    const Particle &p = aos[i];
    soa.x[i] = p.x;
    soa.y[i] = p.y;
    soa.z[i] = p.z;
    soa.vx[i] = p.vx;
    soa.vy[i] = p.vy;
    soa.vz[i] = p.vz;
    soa.mass[i] = p.mass;
    soa.circumference[i] = p.circumference;
  }
  return soa;
}

void queue_correctness_test() {
  constexpr std::size_t kCap = 1024;
  auto queue = SPSCQueue<float, kCap>{};
  auto push_fun = [&queue = queue]() -> std::size_t {
    for (const float i : std::ranges::iota_view(std::size_t{0}, kCap)) {
      queue.push(i);
    }
    return queue.num_elements();
  };

  auto puret = std::async(std::launch::async, push_fun);
  auto ne_push = puret.get();
  std::cout << "Push num elements: " << ne_push << std::endl;

  auto pop_fun = [&queue = queue]() -> std::size_t {
    float f;
    for (const auto i : std::ranges::iota_view(std::size_t{0}, kCap)) {
      queue.pop(f);
      if (static_cast<float>(i) != f) {
        throw std::runtime_error("values don't match");
      }
    }
    return queue.num_elements();
  };

  auto poret = std::async(std::launch::async, pop_fun);
  auto ne_pop = poret.get();
  std::cout << "Pop num elements: " << ne_pop << std::endl;
}

void queue_correctness_test_concurrent() {
  constexpr std::size_t kCap = 1024;
  auto queue = SPSCQueue<float, kCap>{};
  auto push_fun = [&queue = queue]() -> std::size_t {
    for (const float i : std::ranges::iota_view(std::size_t{0}, kCap)) {
      queue.push(i);
    }
    return queue.num_elements();
  };

  auto pop_fun = [&queue = queue]() -> std::size_t {
    float f;
    for (const auto i : std::ranges::iota_view(std::size_t{0}, kCap)) {
      queue.pop(f);
      if (static_cast<float>(i) != f) {
        throw std::runtime_error("values don't match");
      }
    }
    return queue.num_elements();
  };

  std::cout << "Concurrent correctness test" << std::endl;
  auto puret = std::async(std::launch::async, push_fun);
  auto poret = std::async(std::launch::async, pop_fun);
  auto ne_push = puret.get();
  auto ne_pop = poret.get();
  std::cout << "Push num elements: " << ne_push << std::endl;
  std::cout << "Pop num elements: " << ne_pop << std::endl;
}

} // namespace

// Spin for ~200ms before measuring. Without this the benchmark window runs
// at an unknown clock state (frequency ramping / DVFS), which can distort
// sub-100ns comparisons by 2x.
void spin_warmup() {
  volatile std::uint64_t sink = 0;
  const auto freq = bench::timer_freq();
  const auto t0 = bench::timer_start();
  while (bench::timer_start() - t0 < freq / 5) {
    ++sink;
  }
}

// Creation cost of a 1000-element vector: std::allocator vs ArenaAllocator.
// The vector (and with it the allocator's Arena) is constructed inside the
// timed callable: the Arena cannot be reused across iterations because
// deallocate is a no-op and the bump pointer never rewinds until reset().
// The data pointer is returned so the allocation cannot be dead-code elided.
void bench_arena_allocator() {
  constexpr std::size_t kElems = 1000;
  constexpr std::size_t kIters = 10'000;
  spin_warmup();

  const auto std_ticks = bench::benchmark_N<kIters>([] {
    std::vector<int> v(kElems);
    auto *p = v.data();
    bench::keep(p); // sink: allocation result must be materialized
    return v.size();
  });

  const auto arena_ticks = bench::benchmark_N<kIters>([] {
    // Arena holds exactly kElems ints: vector(kElems) performs a single
    // allocate(kElems), which fits the Arena exactly.
    std::vector<int, ArenaAllocator<int, kElems>> v(kElems);
    auto *p = v.data();
    bench::keep(p);
    return v.size();
  });

  const auto freq = static_cast<double>(bench::timer_freq());
  const auto per_op_ns = [freq](std::uint64_t ticks) {
    return static_cast<double>(ticks) / kIters / freq * 1e9;
  };

  std::cout << "create vector<int> x" << kElems << " (" << kIters
            << " iterations)\n"
            << "  std::allocator: " << std_ticks << " ticks ("
            << per_op_ns(std_ticks) << " ns/creation)\n"
            << "  ArenaAllocator:  " << arena_ticks << " ticks ("
            << per_op_ns(arena_ticks) << " ns/creation)\n";
}

// Realistic arena use case: the Arena is created once at program start and
// reused across the hot path. Each iteration is one "frame": construct a
// vector from the persistent allocator, fill it, destroy it (deallocate is a
// no-op), then rewind the Arena with reset(). The hot path performs no heap
// allocation; the only per-iteration costs are the bump allocation, the
// shared_ptr refcount traffic of the allocator copy, and the rewind.
void bench_arena_allocator_hotpath() {
  constexpr std::size_t kElems = 1000;
  constexpr std::size_t kIters = 10'000;
  spin_warmup();

  // Created once, outside the timed hot path.
  ArenaAllocator<int, kElems> alloc;

  const auto std_ticks = bench::benchmark_N<kIters>([] {
    std::vector<int> v(kElems);
    auto *p = v.data();
    bench::keep(p);
    return v.size();
  });

  const auto arena_ticks = bench::benchmark_N<kIters>([&alloc] {
    std::vector<int, ArenaAllocator<int, kElems>> v(alloc);
    v.resize(kElems);
    auto *p = v.data();
    bench::keep(p);
    auto size = v.size();
    alloc.reset(); // rewind for the next iteration
    return size;
  });

  const auto freq = static_cast<double>(bench::timer_freq());
  const auto per_op_ns = [freq](std::uint64_t ticks) {
    return static_cast<double>(ticks) / kIters / freq * 1e9;
  };

  std::cout << "hotpath vector<int> x" << kElems << " (" << kIters
            << " iterations, arena created once)\n"
            << "  std::allocator: " << std_ticks << " ticks ("
            << per_op_ns(std_ticks) << " ns/frame)\n"
            << "  ArenaAllocator:  " << arena_ticks << " ticks ("
            << per_op_ns(arena_ticks) << " ns/frame)\n";
}

void bench_arena_vector() {
  constexpr std::size_t num_elements{1'000'000};
  auto random_particles = make_particles_aos(num_elements, 34);

  auto bench_vec_new = [&random_particles = random_particles]() {
    std::vector<Particle> v_new;
    // v_new.reserve(num_elements);
    for (auto &i : random_particles) {
      v_new.push_back(i);
    }
    return v_new.size();
  };

  auto my_pow = [](this auto &&self, double base, double exp) constexpr {
    if (exp == 0.0)
      return 1.0;
    return base * self(base, exp - 1.0);
  };
  // push_back doubles the capacity: 1, 2, 4, ... up to 2^20 (the first power
  // of two >= num_elements). deallocate() is a no-op, so the arena must hold
  // the sum of every growth buffer: 2^0 + ... + 2^20 = 2^21 - 1 elements.
  constexpr auto vec_arena_size = static_cast<std::size_t>(my_pow(2.0, 21.0));

  auto arena_alloc = ArenaAllocator<Particle, vec_arena_size>();

  auto bench_vec_arena = [&random_particles = random_particles,
                          &arena = arena_alloc]() {
    std::vector<Particle, ArenaAllocator<Particle, vec_arena_size>> v_new(
        arena);
    // v_new.reserve(num_elements);

    for (auto &i : random_particles) {
      v_new.push_back(i);
    }
    return v_new.size();
  };

  auto [res_new, time_new] = bench::il_benchmark(bench_vec_new);
  auto [res_arena, time_arena] = bench::il_benchmark(bench_vec_arena);

  std::cout << "Bench arena vector:\n"
            << "\nres_new: " << res_new << "\t time_new: " << time_new
            << "\nres_arena: " << res_arena << "\t time_arena: " << time_arena
            << std::endl;
}

// 1M subtract-accumulate operations (acc += a[i] - b[i]) in three formats:
//   - double                 : scalar FADD chain (no reassociation without
//                              fast-math, so the compiler cannot vectorize)
//   - FixedPoint<int64_t, 8> : integer pipeline under the hood
//   - int64_t raw            : the fixed-point raw values with no wrapper;
//                              must match FixedPoint tick-for-tick (zero-cost
//                              abstraction check)
// All formats hold the same logical values (fixed seed, generated at runtime
// so nothing can be constant-folded). Each timed iteration is one pass over
// the 1M-element arrays; the accumulator is returned and sunk by keep().
void bench_fixed_point_vs_double() {
  using Fix = FixedPoint<std::int64_t, 8>;
  constexpr std::size_t kOps = 1'000;
  constexpr std::size_t kIters = 10;
  spin_warmup();

  std::mt19937_64 rng(kGenSeed);
  std::uniform_real_distribution<double> dist{-1000.0, 1000.0};
  std::vector<double> da(kOps), db(kOps);
  std::vector<Fix> fa(kOps), fb(kOps);
  std::vector<std::int64_t> ia(kOps), ib(kOps);
  for (std::size_t i = 0; i < kOps; ++i) {
    da[i] = dist(rng);
    db[i] = dist(rng);
    fa[i] = Fix(da[i]);
    fb[i] = Fix(db[i]);
    ia[i] = fa[i].raw(); // identical quantization as the FixedPoint arrays
    ib[i] = fb[i].raw();
  }

  const auto dbl_ticks = bench::benchmark_N<kIters>([&] {
    double acc = 0.0;
    for (std::size_t i = 0; i < kOps; ++i) {
      acc += da[i] - db[i];
    }
    return acc;
  });

  const auto fix_ticks = bench::benchmark_N<kIters>([&] {
    auto acc = Fix(0);
    for (std::size_t i = 0; i < kOps; ++i) {
      acc = acc + (fa[i] - fb[i]);
    }
    return acc.raw();
  });

  const auto int_ticks = bench::benchmark_N<kIters>([&] {
    std::int64_t acc = 0;
    for (std::size_t i = 0; i < kOps; ++i) {
      acc += ia[i] - ib[i];
    }
    return acc;
  });

  const auto freq = static_cast<double>(bench::timer_freq());
  const auto ns_per_op = [freq](std::uint64_t ticks) {
    return static_cast<double>(ticks) / kIters / freq * 1e9 / kOps;
  };

  std::cout << "subtract-accumulate, " << kOps << " ops/pass (" << kIters
            << " iterations)\n"
            << "  double:                 " << dbl_ticks << " ticks ("
            << ns_per_op(dbl_ticks) << " ns/op)\n"
            << "  FixedPoint<int64_t, 8>: " << fix_ticks << " ticks ("
            << ns_per_op(fix_ticks) << " ns/op)\n"
            << "  int64_t raw:            " << int_ticks << " ticks ("
            << ns_per_op(int_ticks) << " ns/op)\n";
}

int main() {
  constexpr auto NUM_PARTICLES_SMALL = 1'000;
  constexpr auto NUM_PARTICLES_MEDIUM = 10'000;
  constexpr auto NUM_PARTICLES_LARGE = 100'000;
  constexpr auto NUM_PARTICLES_XLARGE = 1'000'000;

  const auto gen = [](std::size_t n) {
    auto aos = make_particles_aos(n, kGenSeed);
    auto soa = make_particles_soa(n, kGenSeed);
    auto soa_conv = make_particles_soa_from_aos(aos);
    // The conversion must reproduce the seeded SoA exactly, field by field.
    const bool converted_matches_seeded =
        std::ranges::equal(soa.x, soa_conv.x) &&
        std::ranges::equal(soa.y, soa_conv.y) &&
        std::ranges::equal(soa.z, soa_conv.z) &&
        std::ranges::equal(soa.vx, soa_conv.vx) &&
        std::ranges::equal(soa.vy, soa_conv.vy) &&
        std::ranges::equal(soa.vz, soa_conv.vz) &&
        std::ranges::equal(soa.mass, soa_conv.mass) &&
        std::ranges::equal(soa.circumference, soa_conv.circumference);
    std::cout << "n=" << n << "\taos.back().x=" << aos.back().x
              << "\tsoa.x.back=" << soa.x.back()
              << "\tsoa_conv.x.back=" << soa_conv.x.back()
              << "\tconverted==seeded: "
              << (converted_matches_seeded ? "yes" : "NO") << "\n";
    return std::tuple(aos, soa);
  };

  auto [aos_SMALL, soa_SMALL] = gen(NUM_PARTICLES_SMALL);
  const auto [aos_res, aos_ticks] = bench::il_benchmark(
      [](std::vector<Particle> &p, double dt) {
        update_particle_aos(p, dt);
        return p.size();
      },
      aos_SMALL, 0.01);

  const auto [soa_res, soa_ticks] = bench::il_benchmark(
      [](SoaParticle &soa, double dt) {
        update_particle_soa(soa, dt);
        return soa.x.size();
      },
      soa_SMALL, 0.01);
  std::cout << "update_particle_aos: " << aos_ticks
            << " ticks (size=" << aos_res << ")\n";

  std::cout << "update_particle_soa: " << soa_ticks
            << " ticks (size=" << soa_res << ")\n";

  auto [aos_MEDIUM, soa_MEDIUM] = gen(NUM_PARTICLES_MEDIUM);
  const auto [aos_res_medium, aos_ticks_medium] = bench::il_benchmark(
      [](std::vector<Particle> &p, double dt) {
        update_particle_aos(p, dt);
        return p.size();
      },
      aos_MEDIUM, 0.01);

  const auto [soa_res_medium, soa_ticks_medium] = bench::il_benchmark(
      [](SoaParticle &soa, double dt) {
        update_particle_soa(soa, dt);
        return soa.x.size();
      },
      soa_MEDIUM, 0.01);
  std::cout << "update_particle_aos: " << aos_ticks_medium
            << " ticks (size=" << aos_res_medium << ")\n";

  std::cout << "update_particle_soa: " << soa_ticks_medium
            << " ticks (size=" << soa_res_medium << ")\n";

  auto [aos_LARGE, soa_LARGE] = gen(NUM_PARTICLES_LARGE);
  const auto [aos_res_large, aos_ticks_large] = bench::il_benchmark(
      [](std::vector<Particle> &p, double dt) {
        update_particle_aos(p, dt);
        return p.size();
      },
      aos_LARGE, 0.01);

  const auto [soa_res_large, soa_ticks_large] = bench::il_benchmark(
      [](SoaParticle &soa, double dt) {
        update_particle_soa(soa, dt);
        return soa.x.size();
      },
      soa_LARGE, 0.01);
  std::cout << "update_particle_aos: " << aos_ticks_large
            << " ticks (size=" << aos_res_large << ")\n";

  std::cout << "update_particle_soa: " << soa_ticks_large
            << " ticks (size=" << soa_res_large << ")\n";

  auto [aos_XLARGE, soa_XLARGE] = gen(NUM_PARTICLES_XLARGE);
  const auto [aos_res_xlarge, aos_ticks_xlarge] = bench::il_benchmark(
      [](std::vector<Particle> &p, double dt) {
        update_particle_aos(p, dt);
        return p.size();
      },
      aos_XLARGE, 0.01);

  const auto [soa_res_xlarge, soa_ticks_xlarge] = bench::il_benchmark(
      [](SoaParticle &soa, double dt) {
        update_particle_soa(soa, dt);
        return soa.x.size();
      },
      soa_XLARGE, 0.01);
  std::cout << "update_particle_aos: " << aos_ticks_xlarge
            << " ticks (size=" << aos_res_xlarge << ")\n";

  std::cout << "update_particle_soa: " << soa_ticks_xlarge
            << " ticks (size=" << soa_res_xlarge << ")\n";

  bench_arena_allocator();
  bench_arena_allocator_hotpath();
  bench_arena_vector();
  bench_fixed_point_vs_double();

  auto queue = SPSCQueue<float, 1024>{};
  auto push_fun = [&queue = queue]() -> std::size_t {
    for (const float i : std::ranges::iota_view(0, NUM_PARTICLES_MEDIUM)) {
      queue.push(i);
    }
    return queue.num_elements();
  };
  auto pop_fun = [&queue = queue]() -> std::size_t {
    float f;
    for ([[maybe_unused]] const auto i :
         std::ranges::iota_view(0, NUM_PARTICLES_MEDIUM)) {
      queue.pop(f);
    }
    return queue.num_elements();
  };

  auto puret = std::async(std::launch::async, push_fun);
  auto poret = std::async(std::launch::async, pop_fun);
  queue_correctness_test();
  queue_correctness_test_concurrent();

  auto place_holder = 0;
  auto [res, meas_time] = bench::il_benchmark(
      [&puret, &poret]([[maybe_unused]] int _placeholder) {
        auto ne_push = puret.get();
        auto ne_pop = poret.get();
        return std::make_tuple(ne_push, ne_pop);
      },
      place_holder);

  std::cout << "Push num elements: " << std::get<0>(res)
            << "\tPop num elements: " << std::get<1>(res) << std::endl;
  std::cout << "Time spent: " << meas_time << std::endl;
  return 0;
}
