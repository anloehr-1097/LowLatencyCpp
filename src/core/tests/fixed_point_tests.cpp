#include "fixed_point.h"
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <type_traits>

using F4 = FixedPoint<std::int32_t, 4>;
using F8 = FixedPoint<std::int32_t, 8>;

// --- compile-time guarantees ----------------------------------------------

// const operands must yield a FixedPoint, not decay to double via the
// implicit conversion operator.
static_assert(
    std::is_same_v<
        decltype(std::declval<const F8>() + std::declval<const F8>()), F8>);

// Mixed-precision addition yields the coarser scale (fewer fractional bits).
static_assert(
    std::is_same_v<
        decltype(std::declval<const F8>() + std::declval<const F4>()), F4>);
static_assert(
    std::is_same_v<
        decltype(std::declval<const F4>() + std::declval<const F8>()), F4>);

// Mixed-precision subtraction yields the coarser scale (fewer fractional bits).
static_assert(
    std::is_same_v<
        decltype(std::declval<const F8>() - std::declval<const F8>()), F8>);
static_assert(
    std::is_same_v<
        decltype(std::declval<const F8>() - std::declval<const F4>()), F4>);
static_assert(
    std::is_same_v<
        decltype(std::declval<const F4>() - std::declval<const F8>()), F4>);

// Mixed-precision division yields the coarser scale (fewer fractional bits).
static_assert(
    std::is_same_v<
        decltype(std::declval<const F8>() / std::declval<const F8>()), F8>);
static_assert(
    std::is_same_v<
        decltype(std::declval<const F8>() / std::declval<const F4>()), F4>);
static_assert(
    std::is_same_v<
        decltype(std::declval<const F4>() / std::declval<const F8>()), F4>);

// Construction and addition are usable in constant expressions.
static_assert(static_cast<double>(F8(2) + F8(3)) == 5.0);
static_assert(static_cast<double>(F8(2.5) + F8(1.25)) == 3.75);
static_assert(static_cast<double>(F8(1.5) + F4(2.25)) == 3.75);
static_assert(static_cast<double>(F4(2.25) + F8(1.5)) == 3.75);

// Subtraction (including negative results) is usable in constant expressions.
static_assert(static_cast<double>(F8(5) - F8(3)) == 2.0);
static_assert(static_cast<double>(F8(2.5) - F8(1.25)) == 1.25);
static_assert(static_cast<double>(F8(1.5) - F8(2.25)) == -0.75);
static_assert(static_cast<double>(F8(-1.5) - F8(2.25)) == -3.75);
static_assert(static_cast<double>(F8(1.5) - F4(2.25)) == -0.75);
static_assert(static_cast<double>(F4(2.25) - F8(1.5)) == 0.75);

TEST(FixedPointMathCreation, CreateFromIntegral) {
  constexpr std::size_t FLOAT_BITS = 8;
  FixedPoint<std::int16_t, FLOAT_BITS> f{9};
  std::int16_t plain_from_integral = 9 << FLOAT_BITS;
  EXPECT_EQ(f.raw(), plain_from_integral);
};

TEST(FixedPointMathCreation, CreateFromDouble) {

  FixedPoint<std::int16_t, 6> f{9.5};
  EXPECT_TRUE(static_cast<double>(f) == 9.5);
}

TEST(FixedPointMathCreation, DefaultConstructedIsZero) {
  FixedPoint<std::int32_t, 8> f;
  EXPECT_EQ(f.raw(), 0);
  EXPECT_DOUBLE_EQ(static_cast<double>(f), 0.0);
}

TEST(FixedPointMathCreation, EqualityComparesRawValues) {
  EXPECT_EQ(F8(2), F8(2));
  EXPECT_NE(F8(2), F8(3));
}

TEST(FixedPointMathCreation, WideBaseTypeWithManyFractionalBits) {
  // The scale factor must be computed in BaseType: a 32-bit `1 << 40` is UB.
  FixedPoint<std::int64_t, 40> one{1.0};
  EXPECT_EQ(one.raw(), std::int64_t{1} << 40);
  EXPECT_DOUBLE_EQ(static_cast<double>(one), 1.0);
  FixedPoint<std::int64_t, 40> two_and_half{2.5};
  EXPECT_DOUBLE_EQ(static_cast<double>(two_and_half), 2.5);
}

TEST(FixedPointMathAddition, SamePrecisionConstOperands) {
  const F8 a(1.5);
  const F8 b(2.25);
  const auto sum = a + b;
  EXPECT_DOUBLE_EQ(static_cast<double>(sum), 3.75);
}

TEST(FixedPointMathAddition, MixedPrecisionBothOrders) {
  const F8 fine(1.5);
  const F4 coarse(2.25);
  EXPECT_DOUBLE_EQ(static_cast<double>(fine + coarse), 3.75);
  EXPECT_DOUBLE_EQ(static_cast<double>(coarse + fine), 3.75);
}

TEST(FixedPointMathAddition, MixedPrecisionTruncatesFinerOperand) {
  // Current policy: the result keeps the coarser scale and the finer
  // operand is truncated. 1.99609375 (raw 511 at scale 8) loses 4 bits.
  const F8 fine(1.99609375);
  const F4 zero(0);
  EXPECT_DOUBLE_EQ(static_cast<double>(zero + fine), 1.9375);
  // Arithmetic right shift rounds negative values toward -inf.
  const F8 neg_fine(-1.99609375);
  EXPECT_DOUBLE_EQ(static_cast<double>(zero + neg_fine), -2.0);
}

TEST(FixedPointMathSubtraction, SamePrecisionConstOperands) {
  const F8 a(2.25);
  const F8 b(1.5);
  const auto diff = a - b;
  EXPECT_DOUBLE_EQ(static_cast<double>(diff), 0.75);
}

TEST(FixedPointMathSubtraction, NegativeResultAndOperands) {
  // Two's complement handles the signs: no special-casing required.
  const F8 a(1.5);
  const F8 b(2.25);
  EXPECT_DOUBLE_EQ(static_cast<double>(a - b), -0.75);
  const F8 neg_a(-1.5);
  EXPECT_DOUBLE_EQ(static_cast<double>(neg_a - b), -3.75);
  EXPECT_DOUBLE_EQ(static_cast<double>(b - neg_a), 3.75);
}

TEST(FixedPointMathSubtraction, MixedPrecisionBothOrders) {
  const F8 fine(2.25);
  const F4 coarse(1.5);
  EXPECT_DOUBLE_EQ(static_cast<double>(fine - coarse), 0.75);
  EXPECT_DOUBLE_EQ(static_cast<double>(coarse - fine), -0.75);
}

TEST(FixedPointMathSubtraction, MixedPrecisionTruncatesFinerOperand) {
  // Same truncation policy as addition. 1.99609375 (raw 511 at scale 8)
  // loses 4 bits: 0 - (511 >> 4) = -31 at scale 4.
  const F8 fine(1.99609375);
  const F4 zero(0);
  EXPECT_DOUBLE_EQ(static_cast<double>(zero - fine), -1.9375);
  // Arithmetic right shift rounds toward -inf: -511 >> 4 == -32.
  const F8 neg_fine(-1.99609375);
  EXPECT_DOUBLE_EQ(static_cast<double>(zero - neg_fine), 2.0);
}

TEST(FixedPointMathMultiplication, SamePrecisionMultiplication) {
  const F8 op1(2.5);
  const F8 op2(2.5);
  const F8 op_res(6.25);
  auto res = op1 * op2;
  EXPECT_EQ(res, op_res);
}

TEST(FixedPointMathDivision, SamePrecisionDivision) {
  const F8 op1(7.5);
  const F8 op2(2.5);
  const F8 op_res(3.0);
  auto res = op1 / op2;
  EXPECT_EQ(res, op_res);
}

TEST(FixedPointMathDivision, NegativeResultAndOperands) {
  const F8 a(7.5);
  const F8 b(2.5);
  EXPECT_DOUBLE_EQ(static_cast<double>(a / b), 3.0);
  const F8 neg_a(-7.5);
  EXPECT_DOUBLE_EQ(static_cast<double>(neg_a / b), -3.0);
  EXPECT_DOUBLE_EQ(static_cast<double>(a / (b - F8(5))), -3.0);
}

TEST(FixedPointMathDivision, MixedPrecisionBothOrders) {
  const F8 fine(3.0);
  const F4 coarse(1.5);
  EXPECT_DOUBLE_EQ(static_cast<double>(fine / coarse), 2.0);
  EXPECT_DOUBLE_EQ(static_cast<double>(coarse / fine), 0.5);
}

TEST(FixedPointMathDivision, MixedPrecisionTruncatesFinerOperand) {
  // Same truncation policy as multiplication. 1.99609375 (raw 511 at
  // scale 8) loses 4 bits: (1 << 8 << 4 >> 4) / (511 >> 4) = 256 / 31.
  const F8 fine(1.99609375);
  const F4 one(1);
  EXPECT_EQ(one / fine, F4::from_raw(256 / 31));
}
