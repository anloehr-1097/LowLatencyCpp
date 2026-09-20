#ifndef ARENA_H
#define ARENA_H

#include <cstddef>
#include <cstdlib>
#include <new>

/*
 * Memory Arena. Return pointers from within pre-allocated memory region.
 * Simple bump allocator to manage memory pre-allocated.
 */
template <std::size_t ArenaSize> class Arena {
  std::size_t pos{0};

public:
  void *mem = nullptr;

  // Ctor. Allocate one large block of memory on the heap.
  Arena<ArenaSize>() {
    auto ptr = malloc(ArenaSize);
    if (ptr != nullptr) {
      mem = ptr;
    } else {
      throw std::bad_alloc();
    }
  };

  // Dtor. Free memory alloced.
  ~Arena() {
    if (mem != nullptr)
      free(mem);
  };

  // From preallocated memory block, return ptr to address and increase
  // position.
  void *allocate(std::size_t size, std::size_t alignment) {
    // caller needs to make sure no nullptr returned
    if (mem == nullptr) {
      return nullptr;
    }
    auto padding = (alignment - (pos % alignment)) % alignment;
    auto aligned_pos = pos + padding;

    if (aligned_pos + size > ArenaSize) {
      return nullptr;
    } else
      pos = aligned_pos + size;
    return static_cast<std::byte *>(mem) + aligned_pos;
  };
  void reset() { pos = 0; };

  // True if p points into the memory region owned by this Arena.
  bool owns(const void *p) const {
    auto b = static_cast<const std::byte *>(p);
    return b >= static_cast<const std::byte *>(mem) &&
           b < static_cast<const std::byte *>(mem) + ArenaSize;
  };
};

#endif // ARENA_H
