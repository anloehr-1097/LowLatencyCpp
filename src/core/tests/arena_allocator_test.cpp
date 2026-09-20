#include "ArenaAllocator.h"

#include <cstddef>
#include <gtest/gtest.h>
#include <type_traits>
#include <vector>

TEST(ArenaAllocatorSimpleUse, ArenaAllocatorWithVec) {
  constexpr std::size_t vec_size = 1'000;
  using Alloc = ArenaAllocator<int, vec_size>;

  auto vec = std::vector<int, Alloc>{};

  // Compile-time: the vector's allocator type is the ArenaAllocator, not the
  // default allocator. Containers must allocate through allocator_traits of
  // exactly this type.
  static_assert(std::is_same_v<typename decltype(vec)::allocator_type, Alloc>);

  // Runtime: the container is bound to our Arena instance (identity), not
  // merely to an allocator of the same type.
  auto alloc = vec.get_allocator();
  ASSERT_TRUE(alloc == vec.get_allocator());
  ASSERT_TRUE(alloc != Alloc{});

  vec.push_back(1);
  vec.push_back(2);
  ASSERT_EQ(vec.size(), static_cast<std::size_t>(2));

  // Physical proof: the elements live inside the Arena's memory block.
  ASSERT_TRUE(alloc.owns(vec.data()));
};
