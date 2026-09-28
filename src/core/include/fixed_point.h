#ifndef FIXED_POINT_H
#define FIXED_POINT_H

#include <cstddef>
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

  constexpr FixedPoint(BaseType x) { raw_ = x << FractionalBits; };
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

private:
  BaseType raw_{};

  // operator + - * /
};

#endif // FIXED_POINT_H
