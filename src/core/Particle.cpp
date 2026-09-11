#include "include/Particle.h"
#include <ranges>

void update_particle_soa(SoaParticle &soa, double dt) {
  auto update_pos = [](double &coord, double &speed, double dt) -> void {
    coord += speed * dt;
  };

  // Note: zip yields a prvalue pair<double&, double&>, so plain `auto&`
  // binding is ill-formed; `auto&&` binds the prvalue while x/vx still
  // alias the vector elements (mutation writes through).
  for (auto&& [x, vx] : std::ranges::views::zip(soa.x, soa.vx)) {
    update_pos(x, vx, dt);
  }

  for (auto&& [y, vy] : std::ranges::views::zip(soa.y, soa.vy)) {
    update_pos(y, vy, dt);
  }
  for (auto&& [z, vz] : std::ranges::views::zip(soa.z, soa.vz)) {
    update_pos(z, vz, dt);
  }
}

void update_particle_aos(std::vector<Particle> &vec, double dt) {
  for (auto &p : vec) {
    p.x += p.vx * dt;
    p.y += p.vy * dt;
    p.z += p.vz * dt;
  }
};
