#include <gtest/gtest.h>

#include <Particle.h>

TEST(ParticleAos, UpdatesPositionsFromVelocities) {
  std::vector<Particle> vec{{.x = 0, .y = 1, .z = 2,
                             .vx = 1, .vy = -2, .vz = 0.5,
                             .mass = 1, .circumference = 0}};
  update_particle_aos(vec, 0.5);

  EXPECT_DOUBLE_EQ(vec[0].x, 0.5);
  EXPECT_DOUBLE_EQ(vec[0].y, 0.0);
  EXPECT_DOUBLE_EQ(vec[0].z, 2.25);
  EXPECT_DOUBLE_EQ(vec[0].vx, 1.0);
}

TEST(ParticleAos, UpdatesAllParticles) {
  std::vector<Particle> vec(4, Particle{});
  update_particle_aos(vec, 1.0);
  for (const auto &p : vec) {
    EXPECT_DOUBLE_EQ(p.x, 0.0);
    EXPECT_DOUBLE_EQ(p.y, 0.0);
    EXPECT_DOUBLE_EQ(p.z, 0.0);
  }
}

TEST(ParticleSoa, UpdatesPositionsFromVelocities) {
  SoaParticle soa;
  soa.x = {0.0, 1.0};
  soa.vx = {2.0, -1.0};
  soa.y = {5.0};
  soa.vy = {1.0};
  soa.z = {0.0};
  soa.vz = {4.0};

  update_particle_soa(soa, 0.5);

  EXPECT_DOUBLE_EQ(soa.x[0], 1.0);
  EXPECT_DOUBLE_EQ(soa.x[1], 0.5);
  EXPECT_DOUBLE_EQ(soa.y[0], 5.5);
  EXPECT_DOUBLE_EQ(soa.z[0], 2.0);
  EXPECT_DOUBLE_EQ(soa.vx[0], 2.0);
}
