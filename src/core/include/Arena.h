#ifndef ARENA_H
#define ARENA_H
/*
1. **`core/arena.h`** — monotonic bump allocator:
   - Constructed with a fixed-size byte buffer (stack-allocated or `mmap`'d)
   - `void* allocate(size_t size, size_t alignment)` — bumps a pointer, returns
aligned storage
   - `void reset()` — resets the pointer to the start (bulk deallocation)
   - No individual `free()` — the entire arena is freed at once
   - Overflow detection: returns `nullptr` or throws if exhausted
   */

#include <cstddef>
#include <cstdlib>
template <std::size_t ArenaSize> class ArenaBumpAllocator {
  std::size_t pos{0};

public:
  void *mem = nullptr;
  ArenaBumpAllocator();
  ~ArenaBumpAllocator();
  void *allocate(std::size_t size, std::size_t alignment);
  void reset();
};

#endif // ARENA_H
