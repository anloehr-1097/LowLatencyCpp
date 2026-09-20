#ifndef ARENA_H
#define ARENA_H

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
