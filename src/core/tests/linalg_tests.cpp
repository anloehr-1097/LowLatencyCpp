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
