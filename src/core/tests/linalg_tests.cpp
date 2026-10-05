#include "linalg.h"
#include "gtest/gtest.h"

TEST(VectorConstruction, DefaultConstruction) {
  VectNd<float, 3> vec;
  ASSERT_EQ(vec.data[0], 0);
}

TEST(VectorConstruction, InitListConstruction) {
  VectNd<float, 3> vec;
  ASSERT_EQ(vec.data[0], 0);

  auto ls = {1, 2, 3};
  VectNd<int, 3> vec2{ls};
  ASSERT_EQ(vec2.data[0], 1);
}

TEST(VectorConstruction, PerfectForwardConstruction) {
  VectNd<float, 3> vec;
  ASSERT_EQ(vec.data[0], 0);

  VectNd<int, 3> vec2(4, 2, 3);
  ASSERT_EQ(vec2.data[0], 4);
}

TEST(VectorOps, VectorDivision) {
  VectNd<int, 3> vec;

  VectNd<int, 3> vec2(4, 2, 3);

  ASSERT_EQ(vec / vec2, vec);
  ASSERT_NE(vec / vec2, vec2);
}

// Kept out of line so the operands are opaque to the optimizer (no constant
// folding) and the SIMD loop is easy to find in the disassembly.
[[gnu::noinline]] VectNd<float, 12> add_f32x12(const VectNd<float, 12> &lhs,
                                               const VectNd<float, 12> &rhs) {
  return lhs + rhs;
}

TEST(VectorOps, VectorAdditionSimdFloat12) {
  const VectNd<float, 12> lhs(1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f,
                              9.0f, 10.0f, 11.0f, 12.0f);
  const VectNd<float, 12> rhs(10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f,
                              80.0f, 90.0f, 100.0f, 110.0f, 120.0f);

  const VectNd<float, 12> sum = add_f32x12(lhs, rhs);

  // Elements 0-7 go through the 8-wide SIMD path, 8-11 through the scalar tail.
  for (std::size_t i = 0; i < 12; ++i) {
    EXPECT_FLOAT_EQ(sum.data[i], 11.0f * static_cast<float>(i + 1)) << "i=" << i;
  }
}
