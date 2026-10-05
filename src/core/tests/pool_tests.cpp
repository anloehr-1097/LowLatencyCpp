#include <Pool.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <new>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

/*
 * Global new/delete overrides that count every free-store allocation in the
 * whole test binary. Tests that must prove no hidden heap traffic occurs (i.e.
 * the pool hands out pre-allocated slots without ever touching operator new)
 * bracket their work in an AllocRecorder guard and assert zero counts.
 *
 * Must live at global scope, NOT in an anonymous namespace: the compiler only
 * routes allocation expressions through the implicitly-declared global
 * operators.
 */
std::size_t g_alloc_count = 0;
std::size_t g_free_count = 0;

void *operator new(std::size_t size) {
  ++g_alloc_count;
  void *p = std::malloc(size);
  if (!p)
    throw std::bad_alloc();
  return p;
}

void operator delete(void *p) noexcept {
  if (p) {
    ++g_free_count;
    std::free(p);
  }
}

void operator delete(void *p, std::size_t) noexcept { operator delete(p); }

void *operator new[](std::size_t size) {
  ++g_alloc_count;
  void *p = std::malloc(size);
  if (!p)
    throw std::bad_alloc();
  return p;
}

void operator delete[](void *p) noexcept {
  if (p) {
    ++g_free_count;
    std::free(p);
  }
}

void operator delete[](void *p, std::size_t) noexcept { operator delete[](p); }

// The nothrow forms must be replaced too: otherwise memory obtained from the
// library's (or the sanitizer's) nothrow new would be released by the
// std::free in our operator delete.
void *operator new(std::size_t size, const std::nothrow_t &) noexcept {
  ++g_alloc_count;
  return std::malloc(size);
}

void *operator new[](std::size_t size, const std::nothrow_t &) noexcept {
  ++g_alloc_count;
  return std::malloc(size);
}

namespace {

/*
 * RAII guard: remembers the allocation counters on construction; tests assert
 * on the delta afterwards (allocs() == 0 means: no heap was touched).
 */
struct AllocRecorder {
  std::size_t allocs_at_start;
  std::size_t frees_at_start;

  AllocRecorder()
      : allocs_at_start(g_alloc_count), frees_at_start(g_free_count) {}

  std::size_t allocs() const { return g_alloc_count - allocs_at_start; }
  std::size_t frees() const { return g_free_count - frees_at_start; }
};

struct Tracked {
  static inline int alive = 0;
  static inline int constructed = 0;
  static inline int destroyed = 0;

  int id;

  explicit Tracked(int id_) : id(id_) {
    ++alive;
    ++constructed;
  }
  ~Tracked() {
    --alive;
    ++destroyed;
  }
};

void reset_tracked() {
  Tracked::alive = 0;
  Tracked::constructed = 0;
  Tracked::destroyed = 0;
}

using StringPool = Pool<std::string, 8>;
using StringHandle = StringPool::Handle;
using SmallTrackedPool = Pool<Tracked, 4>;

} // namespace

TEST(PoolTest, AllocateReturnsNonNullHandle) {
  StringPool pool;
  auto h = pool.allocate();
  EXPECT_NE(h, nullptr);
  EXPECT_EQ(pool.in_use(), 1);
}

TEST(PoolTest, ExhaustionReturnsNullptrHandle) {
  StringPool pool;
  std::vector<StringHandle> handles;
  for (std::size_t i = 0; i < 8; ++i) {
    handles.push_back(pool.allocate());
    EXPECT_NE(handles.back(), nullptr);
  }
  EXPECT_EQ(pool.allocate(), nullptr);
  EXPECT_EQ(pool.in_use(), 8);
}

TEST(PoolTest, AutomaticReturnOnScopeExitDestroysAndRecycles) {
  reset_tracked();
  SmallTrackedPool pool; // pool outlives all handles in this test
  {
    auto h1 = pool.allocate(1);
    ASSERT_NE(h1, nullptr);
    {
      auto h2 = pool.allocate(2);
      ASSERT_NE(h2, nullptr);
      EXPECT_EQ(Tracked::alive, 2);
    } // h2 goes out of scope -> ~Tracked(), slot recycled
    EXPECT_EQ(Tracked::alive, 1);
    EXPECT_EQ(Tracked::destroyed, 1);
    EXPECT_EQ(Tracked::constructed, 2);
  }
  // h1 (holding Tracked(1)) went out of scope here too
  EXPECT_EQ(Tracked::alive, 0);
  EXPECT_EQ(Tracked::destroyed, 2);
}

TEST(PoolTest, RecycledSlotIsReused) {
  StringPool pool;
  auto h1 = pool.allocate();
  ASSERT_NE(h1, nullptr);
  auto *slot1 = h1.get();
  EXPECT_EQ(pool.in_use(), 1);
  pool.deallocate(std::move(h1));
  EXPECT_EQ(pool.in_use(), 0);

  auto h2 = pool.allocate();
  ASSERT_NE(h2, nullptr);
  EXPECT_EQ(h2.get(), slot1); // LIFO free list -> same slot handed back
  EXPECT_EQ(pool.in_use(), 1);
}

TEST(PoolTest, ExplicitDeallocateRunsDestroyerExactlyOnce) {
  reset_tracked();
  SmallTrackedPool pool;
  {
    auto h = pool.allocate(7);
    ASSERT_NE(h, nullptr);
    EXPECT_EQ(h->id, 7);
    EXPECT_EQ(Tracked::alive, 1);
  } // goes out of scope here, not via explicit deallocate
  EXPECT_EQ(Tracked::alive, 0);
  EXPECT_EQ(Tracked::destroyed, 1);
}

TEST(PoolTest, ConstructIntoProvidedMemory) {
  StringPool pool;
  auto h = pool.allocate("pool memory");
  ASSERT_NE(h, nullptr);
  EXPECT_EQ(*h, "pool memory");
  EXPECT_EQ(h->size(), 11u);

  // handle address lies within the pool object's own storage
  const auto *byte_ptr = reinterpret_cast<const std::byte *>(h.get());
  const auto *pool_begin = reinterpret_cast<const std::byte *>(&pool);
  const auto *pool_end = pool_begin + sizeof(pool);
  EXPECT_GE(byte_ptr, pool_begin);
  EXPECT_LE(byte_ptr, pool_end);
}

TEST(PoolTest, AllSlotsAreDistinctAddresses) {
  StringPool pool;
  std::vector<StringHandle> handles;
  std::vector<std::string *> addresses;
  for (std::size_t i = 0; i < 8; ++i) {
    handles.push_back(pool.allocate());
    addresses.push_back(handles.back().get());
  }
  // pairwise distinct
  for (std::size_t i = 0; i < addresses.size(); ++i)
    for (std::size_t j = i + 1; j < addresses.size(); ++j)
      EXPECT_NE(addresses[i], addresses[j]);
}

TEST(PoolTest, HandleIsMoveOnly) {
  static_assert(!std::is_copy_constructible_v<StringHandle>);
  static_assert(!std::is_copy_assignable_v<StringHandle>);
  static_assert(std::is_move_constructible_v<StringHandle>);
  static_assert(std::is_move_assignable_v<StringHandle>);

  StringPool pool;
  auto h = pool.allocate();
  auto h2 = std::move(h);
  EXPECT_EQ(h, nullptr);       // moved-from handle is empty
  EXPECT_EQ(pool.in_use(), 1); // no double recycling
}

TEST(PoolTest, InUseCountTracksLifecycle) {
  StringPool pool;
  EXPECT_EQ(pool.in_use(), 0);
  {
    auto h = pool.allocate();
    EXPECT_EQ(pool.in_use(), 1);
  }
  EXPECT_EQ(pool.in_use(), 0);
}

TEST(PoolTest, DeallocateNullHandleIsSafe) {
  StringPool pool;
  StringHandle empty;
  pool.deallocate(std::move(empty)); // must not crash / corrupt free list
  EXPECT_EQ(pool.in_use(), 0);
  auto h = pool.allocate();
  EXPECT_NE(h, nullptr); // free list still intact
}

/*
 * The core invariant: allocate/deallocate/automatic return must never touch
 * the free store — the pool exists precisely to hand out pre-allocated
 * storage. Every allocation recorded here is a regression.
 */
TEST(PoolTest, AllocateAndReleaseNeverTouchesGlobalHeap) {
  AllocRecorder rec;
  {
    StringPool pool;
    auto h1 = pool.allocate();
    EXPECT_NE(h1, nullptr);
    auto h2 = pool.allocate();
    EXPECT_NE(h2, nullptr);
    EXPECT_GE(rec.allocs(), 0); // counters sane

    // return paths: explicit + automatic
    pool.deallocate(std::move(h1));
    h2.reset();
  }
  EXPECT_EQ(rec.allocs(), 0u) << "allocate/deallocate used global operator new";
  EXPECT_EQ(rec.frees(), 0u) << "deallocate used global operator delete";
}

TEST(PoolTest, ExhaustedPoolAllocateIsAlsoHeapFree) {
  StringPool pool;
  StringHandle handles[8];
  for (auto &h : handles) {
    h = pool.allocate();
    ASSERT_NE(h, nullptr);
  }
  AllocRecorder rec;
  EXPECT_EQ(pool.allocate(), nullptr);
  EXPECT_EQ(rec.allocs(), 0u) << "allocate() on exhausted pool allocated";
}
