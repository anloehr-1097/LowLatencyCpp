#include "ArenaAllocator.h"

#include <cstddef>
#include <gtest/gtest.h>
#include <stdexcept>
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

// Realistic lifecycle: the Arena is created once at "program start" and
// reused across the hot path. Containers allocate from it, deallocation is
// deferred, and reset() rewinds the bump pointer between frames.
TEST(ArenaAllocatorSimpleUse, ArenaAllocatorHotPathReuse) {
  constexpr std::size_t vec_size = 1'000;
  using Alloc = ArenaAllocator<int, vec_size>;

  // "Program start": create the Arena once.
  Alloc alloc;

  // Hot path: repeated create-use-destroy cycles. All frames share the same
  // Arena; every frame's memory physically comes from it.
  for (int frame = 0; frame < 5; ++frame) {
    std::vector<int, Alloc> vec(alloc);
    vec.resize(vec_size);
    ASSERT_TRUE(alloc.owns(vec.data()));
    for (std::size_t i = 0; i < vec_size; ++i) {
      vec[i] = frame;
    }
    ASSERT_EQ(vec.back(), frame);
    // Rewind for the next frame. Note: this invalidates vec's storage; vec
    // must not be touched afterwards (its dtor only no-ops deallocate).
    alloc.reset();
  }

  // Without reset the Arena is exhausted: the bump pointer never rewinds and
  // the next allocation throws bad_alloc.
  {
    std::vector<int, Alloc> vec(alloc);
    vec.resize(vec_size);
    std::vector<int, Alloc> extra(alloc);
    EXPECT_THROW(extra.resize(1), std::bad_alloc);

    // Recovery: after rewinding, allocation works again.
    alloc.reset();
    extra.resize(1);
    EXPECT_EQ(extra.size(), static_cast<std::size_t>(1));
  }
}
