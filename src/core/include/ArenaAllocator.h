#ifndef ARENA_ALLOCATOR_H
#define ARENA_ALLOCATOR_H

#include "Arena.h"
#include <cstddef>
#include <memory>
/*
 * ArenaAllocator to use with STL containers.
 *
 * The Arena is owned via shared_ptr and shared among all copies/rebinds of the
 * same allocated "generation". This is required: STL containers make internal
 * allocator copies (e.g. libc++ __allocation_guard) whose allocations must
 * outlive the copy, so a copy creating its own Arena is not viable, and a
 * copy whose dynamic storage dies with it corrupts the container.
 */
template <typename T, std::size_t N> struct ArenaAllocator {
  using value_type = T;

  /*
   * Ctor.
   */
  ArenaAllocator() = default;

  /*
   * Allocate space for n objects of type T
   */
  T *allocate(std::size_t n) {
    auto ptr = arena->allocate(n * sizeof(T), alignof(T));
    if (!ptr) {
      throw std::bad_alloc();
    }
    return static_cast<T *>(ptr);
  }

  /*
   * Don't deallocate. Must call Arena.reset() to deallocate whole mem.
   */
  void deallocate([[maybe_unused]] T *p,
                  [[maybe_unused]] std::size_t n) noexcept {
    // does nothing. Arena is cleared by calling reset.
    // No deallocation of single objects.
  }

  template <typename U> struct rebind {
    using other = ArenaAllocator<U, N>;
  };

  // Rebinding changes sizeof(U), so a differently-sized Arena is required:
  // create a fresh, new Arena owned by the rebound allocator.
  template <typename U, std::size_t M>
  ArenaAllocator([[maybe_unused]] const ArenaAllocator<U, M> &other)
      : arena(std::make_shared<Arena<N * sizeof(T)>>()) {}

  friend bool operator==(const ArenaAllocator &a,
                         const ArenaAllocator &b) noexcept {
    return a.arena == b.arena;
  }
  friend bool operator!=(const ArenaAllocator &a,
                         const ArenaAllocator &b) noexcept {
    return !(a == b);
  }

  /*
   * Warning: resetting the Arena invalidates every object allocated through
   * any allocator sharing it.
   */
  void reset() noexcept { arena->reset(); }

  /*
   * True if p points into the Arena shared by this allocator.
   */
  bool owns(const T *p) const noexcept { return arena->owns(p); }

private:
  std::shared_ptr<Arena<N * sizeof(T)>> arena =
      std::make_shared<Arena<N * sizeof(T)>>();
};

#endif // ARENA_ALLOCATOR_H
