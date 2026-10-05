/*
 * This memory pool has good data localility only when the access pattern is
 * LIFO. The more scattered the used (or free) slots are the worse is the memory
 * access pattern.
 *
 */
#ifndef POOL_H
#define POOL_H

#include <array>
#include <cassert>
#include <cstddef>
#include <memory>
#include <new>
#include <utility>

// hold next free slot or element of type T
template <typename T> union Slot {
  alignas(T) std::byte storage[sizeof(T)];
  Slot *next;
};

template <typename T, std::size_t N> class Pool;

/*
 * Custom deleter: runs ~T() and recycles the slot into the pool's free list.
 * Exactly one place where slot management happens; it runs automatically when
 * the handle goes out of scope (or is moved into Pool::deallocate).
 */
template <typename T, std::size_t N> struct PoolReleaser {
  Pool<T, N> *pool;

  void operator()(T *p) const {
    if (!p)
      return;
    std::destroy_at(p);
    auto *slot = std::launder(reinterpret_cast<Slot<T> *>(p));
    slot->next = pool->free_head;
    pool->free_head = slot;
    --pool->in_use_count;
  }
};

template <typename T, std::size_t N> class Pool {
private:
  std::array<Slot<T>, N> data{};
  Slot<T> *free_head = nullptr;
  std::size_t in_use_count = 0;

  friend struct PoolReleaser<T, N>;

public:
  using Handle = std::unique_ptr<T, PoolReleaser<T, N>>;

  Pool() {
    for (std::size_t i = 0; i < N; ++i) {
      data[i].next = free_head;
      free_head = &data[i];
    }
  }

  /*
   * Take a slot off the free list, construct a T in it from the forwarded
   * arguments and hand it to the caller as an owning handle. Returns nullptr
   * handle when the pool is exhausted.
   */
  template <typename... Args> Handle allocate(Args &&...args) {
    if (!free_head)
      return nullptr;

    auto *slot = free_head;
    // Constructing the T overwrites slot->next, so read it first. The free
    // list is only updated afterwards: if T's ctor throws, the slot is still
    // on the list and nothing leaks.
    auto *next = slot->next;
    T *ptr = ::new (static_cast<void *>(slot->storage))
        T(std::forward<Args>(args)...);
    free_head = next;
    ++in_use_count;
    return Handle(ptr, PoolReleaser<T, N>{this});
  }

  /*
   * Explicit return path: taking ownership is enough. The handle's deleter
   * (~T() + free-list recycle) runs exactly once when the handle goes out of
   * scope here.
   */
  void deallocate(Handle &&h) { Handle moved = std::move(h); }

  /*
   * Records how many slots are currently handed out.
   */
  std::size_t in_use() const { return in_use_count; }

  ~Pool() {
    // Contract: handles must never outlive the pool. Destroy all of them
    // (scope exit / deallocate) before the pool dies.
    assert(in_use() == 0);
  }
};

#endif // POOL_H
