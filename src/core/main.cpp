#include "il_benchmark.hpp"
#include "include/Particle.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <random>
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

} // namespace

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

  return 0;
}
