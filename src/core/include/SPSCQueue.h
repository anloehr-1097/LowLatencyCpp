/**`core/spsc_queue.h`** — a templated, fixed-capacity, lock-free
   single-producer single-consumer queue:
   - Power-of-two capacity (compile-time or runtime, enforced with
   `static_assert` or runtime check)
   - `try_push(const T&) -> bool` and `try_pop(T&) -> bool` — non-blocking
   - `push(const T&)` — spins until space is available (optional, with backoff)
   - Head and tail indices on separate cache lines (padding to avoid false
   sharing)
   - Memory ordering: producer uses `std::memory_order_release` on tail update;
   consumer uses `std::memory_order_acquire` on head read (and vice versa). No
   `seq_cst` anywhere.
   - Storage is a flat array — no heap allocation per element
   */

#if defined(__aarch64__) || defined(__arm__)
#include <arm_acle.h>
#define APause() __yield()
#elif defined(_M_ARM64) || defined(_M_ARM)
#include <intrin.h>
#define APause() __yield()
#elif defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#define APause() _mm_pause()
#elif defined(_M_X64) || defined(_M_IX86)
#include <intrin.h>
#define APause() _mm_pause()
#else
#define APause() ((void)0)
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <new>
constexpr auto CacheLineSize =
    std::max(std::hardware_constructive_interference_size, std::size_t{128});

template <typename T, std::size_t N> class SPSCQueue {
  static constexpr std::size_t capacity{N};

  std::array<T, N> data{};
  struct alignas(CacheLineSize) ConsumerVars {
    std::atomic<std::size_t> head{0}; // pop loc
    std::size_t consumable{0};
    std::size_t cached_tail{0}; // last-known producer tail
  };
  struct alignas(CacheLineSize) ProducerVars {
    std::atomic<std::size_t> tail{0}; // push loc
    alignas(CacheLineSize) std::size_t cached_head{
        0}; // last-known consumer head
    alignas(CacheLineSize) std::size_t free{N};
  };
  ProducerVars p;
  ConsumerVars c;

public:
  SPSCQueue() {
    // power of 2 <=> excatly 1 bit set
    static_assert([](std::size_t n) { return (n > 0) && !(n & (n - 1)); }(N),
                  "Queue size N not a power of 2.");
  }
  void push(const T &);
  void pop(T &);
  std::size_t num_elements();
};
