#include "Arena.h"
#include "SPSCQueue.h"
#include <cstddef>
#include <cstdlib>

template <std::size_t ArenaSize>
ArenaBumpAllocator<ArenaSize>::ArenaBumpAllocator() {
  auto ptr = malloc(ArenaSize);
  if (ptr != nullptr) {
    mem = ptr;
  }
}

template <std::size_t ArenaSize>
ArenaBumpAllocator<ArenaSize>::~ArenaBumpAllocator() {
  if (mem != nullptr)
    free(mem);
};

template <std::size_t ArenaSize>
void *ArenaBumpAllocator<ArenaSize>::allocate(std::size_t size,
                                              std::size_t alignment) {
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

template <std::size_t ArenaSize> void ArenaBumpAllocator<ArenaSize>::reset() {
  pos = 0;
}

template class ArenaBumpAllocator<1024>;
