#ifndef PARTICLE_H
#define PARTICLE_H

#include <cstddef>
#include <vector>
struct Particle {
  double x;
  double y;
  double z;
  double vx;
  double vy;
  double vz;
  double mass;
  double circumference;
};

struct SoaParticle {
  std::vector<double> x;
  std::vector<double> y;
  std::vector<double> z;
  std::vector<double> vx;
  std::vector<double> vy;
  std::vector<double> vz;
  std::vector<double> mass;
  std::vector<double> circumference;
};

void update_particle_aos(std::vector<Particle> &vec, double dt);
void update_particle_soa(SoaParticle &soa, double dt);

#endif // PARTICLE_H
