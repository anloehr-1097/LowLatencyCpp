/*
 * Fixed Point Math with no overflowing guarantees. Supporting basic operations
 * (+ - * /)
 */

#ifndef FIXED_POINT_H
#define FIXED_POINT_H

#include <cstddef>
#include <cstdint>
#include <type_traits>

template <typename T>
concept FloatingPointType = std::is_floating_point_v<T>;

template <typename BaseType, std::size_t FractionalBits> struct FixedPoint {
  template <typename, std::size_t>
  friend struct FixedPoint; // required for operator overloading

  template <FloatingPointType T> constexpr FixedPoint(T x) {
    // Example: NumericType = double
    // (BaseType) (x * (double)(1 << FractionalBits))
    //
    raw_ = static_cast<BaseType>(x *
                                 static_cast<T>(BaseType{1} << FractionalBits));
  }

  constexpr FixedPoint(BaseType x) {
    raw_ = static_cast<BaseType>(
        static_cast<std::common_type_t<BaseType, long long>>(x)
        << FractionalBits);
  };
  FixedPoint() = default;
  static constexpr FixedPoint<BaseType, FractionalBits> from_raw(BaseType x) {
    auto f = FixedPoint();
    f.raw_ = x;
    return f;
  }

  constexpr BaseType raw() const { return raw_; }

  constexpr bool
  operator==(const FixedPoint<BaseType, FractionalBits> &other) const {
    return raw_ == other.raw_;
  }

  constexpr operator double() const {
    return static_cast<double>(raw_) /
           static_cast<double>(BaseType{1} << FractionalBits);
  }

  // this does not deal with overflowing
  constexpr auto
  operator+(const FixedPoint<BaseType, FractionalBits> &other) const {
    // auto f =
    //    FixedPoint<BaseType, FractionalBits - 1>::from_raw(raw_ + other.raw_);
    // No overflow handling here
    auto f = from_raw(raw_ + other.raw_);
    return f;
  }

  /*
   * Basic behavior of addition of instances with differing precision -> take
   * min precision
   */
  template <std::size_t OtherFractionalBits>
  constexpr auto
  operator+(const FixedPoint<BaseType, OtherFractionalBits> &other) const {
    if constexpr (FractionalBits < OtherFractionalBits) {
      auto new_raw =
          raw_ + (other.raw_ >> (OtherFractionalBits - FractionalBits));
      return from_raw(new_raw);
    } else {
      auto shift_bits = FractionalBits - OtherFractionalBits;
      auto new_raw = (raw_ >> (shift_bits)) + other.raw_;
      return FixedPoint<BaseType, OtherFractionalBits>::from_raw(new_raw);
    }
  };

  constexpr auto
  operator-(const FixedPoint<BaseType, FractionalBits> &other) const {
    auto f = from_raw(raw_ - other.raw_);
    return f;
  }

  /*
   * Basic behavior of subtraction of instances with differing precision ->
   * take min precision. The finer operand is truncated via arithmetic right
   * shift (rounds negative values toward -inf), same as operator+.
   */
  template <std::size_t OtherFractionalBits>
  constexpr auto
  operator-(const FixedPoint<BaseType, OtherFractionalBits> &other) const {
    if constexpr (FractionalBits < OtherFractionalBits) {
      auto new_raw =
          raw_ - (other.raw_ >> (OtherFractionalBits - FractionalBits));
      return from_raw(new_raw);
    } else {
      auto shift_bits = FractionalBits - OtherFractionalBits;
      auto new_raw = (raw_ >> (shift_bits)) - other.raw_;
      return FixedPoint<BaseType, OtherFractionalBits>::from_raw(new_raw);
    }
  };

  constexpr auto
  operator*(const FixedPoint<BaseType, FractionalBits> &other) const {
    // prevent overflow by casting to wider type
    auto intermediate = static_cast<std::int64_t>(raw()) * other.raw();
    auto same_precision = static_cast<BaseType>(intermediate >> FractionalBits);
    return from_raw(same_precision);
  }

  /*
   * Basic behavior of multiplication of instances with differing precision ->
   * take min precision. The finer operand is truncated via arithmetic right
   * shift before multiplying, same as operator+.
   */
  template <std::size_t OtherFractionalBits>
  constexpr auto
  operator*(const FixedPoint<BaseType, OtherFractionalBits> &other) const {
    if constexpr (FractionalBits < OtherFractionalBits) {
      auto new_raw = other.raw_ >> (OtherFractionalBits - FractionalBits);
      auto intermediate = static_cast<std::int64_t>(raw_) * new_raw;
      return from_raw(static_cast<BaseType>(intermediate >> FractionalBits));
    } else {
      auto shift_bits = FractionalBits - OtherFractionalBits;
      auto new_raw = (raw_ >> (shift_bits));
      auto intermediate = static_cast<std::int64_t>(new_raw) * other.raw_;
      return FixedPoint<BaseType, OtherFractionalBits>::from_raw(
          static_cast<BaseType>(intermediate >> OtherFractionalBits));
    }
  };

  constexpr auto
  operator/(const FixedPoint<BaseType, FractionalBits> &other) const {
    // prevent overflow by casting to wider type
    auto intermediate =
        (static_cast<std::int64_t>(raw_) << FractionalBits) / other.raw();
    return from_raw(static_cast<BaseType>(intermediate));
  }

  /*
   * Basic behavior of division of instances with differing precision ->
   * take min precision. The finer operand is truncated via arithmetic right
   * shift before dividing, same as operator+.
   */
  template <std::size_t OtherFractionalBits>
  constexpr auto
  operator/(const FixedPoint<BaseType, OtherFractionalBits> &other) const {
    if constexpr (FractionalBits < OtherFractionalBits) {
      auto new_raw = other.raw_ >> (OtherFractionalBits - FractionalBits);
      auto intermediate =
          (static_cast<std::int64_t>(raw_) << FractionalBits) / new_raw;
      return from_raw(static_cast<BaseType>(intermediate));
    } else {
      auto shift_bits = FractionalBits - OtherFractionalBits;
      auto new_raw = (raw_ >> (shift_bits));
      auto intermediate =
          (static_cast<std::int64_t>(new_raw) << OtherFractionalBits) /
          other.raw_;
      return FixedPoint<BaseType, OtherFractionalBits>::from_raw(
          static_cast<BaseType>(intermediate));
    }
  };

private:
  BaseType raw_{};
};

#endif // FIXED_POINT_H
