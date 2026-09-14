#include "include/SPSCQueue.h"
#include <atomic>
#include <cstddef>

/*
 * Comments for blog: We have a *single* consumer, *single* producer queue.
 * At most 2 threads access the queue, we need these 2 threads to be
 * synchronized. We only need to synchronize the head and the tail.
 */

template <typename T, std::size_t N>
  requires PowerOfTwo<N>
void SPSCQueue<T, N>::push(const T &item) {
  // stalling push
  /*
   * Standard pattern: passing a const reference, since it is copy constructed
   * into data array at given postition.
   */
  auto ltail = p.tail.load(std::memory_order_relaxed);
  while (true) {
    if (p.free == 0) {
      p.cached_head = c.head.load(std::memory_order_acquire);
      p.free = capacity - (ltail - p.cached_head);
      if (p.free == 0) {
        APause();
        continue;
      }
    }
    break;
  }

  data[ltail & (capacity - 1)] = item;
  ++ltail;
  p.tail.store(ltail, std::memory_order_release);
  --p.free;
};

template <typename T, std::size_t N>
  requires PowerOfTwo<N>
void SPSCQueue<T, N>::pop(T &val) {
  auto lhead = c.head.load(std::memory_order_relaxed);
  while (true) {
    if (c.consumable == 0) {
      c.cached_tail = p.tail.load(std::memory_order_acquire);
      c.consumable = c.cached_tail - lhead;
      if (c.consumable == 0) {
        APause();
        continue;
      }
    }
    break;
  }
  val = data[lhead & (capacity - 1)];
  ++lhead;
  c.head.store(lhead, std::memory_order_release);
  --c.consumable;
};

template <typename T, std::size_t N>
  requires PowerOfTwo<N>
std::size_t SPSCQueue<T, N>::num_elements() {
  return p.tail.load(std::memory_order_seq_cst) -
         c.head.load(std::memory_order_seq_cst);
};

template class SPSCQueue<float, 1024>;
