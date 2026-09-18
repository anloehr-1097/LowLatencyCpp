#include <gtest/gtest.h>

#include <cstddef>
#include <thread>
#include <vector>

#include <SPSCQueue.h>

// Defined in SPSCQueue.cpp inside the core library.
extern template class SPSCQueue<float, 1024>;

TEST(SPSCQueue, PushPopFifoOrder) {
  SPSCQueue<float, 1024> q;
  for (float i = 0; i < 16; ++i) {
    q.push(i);
  }
  EXPECT_EQ(q.num_elements(), 16);

  float out = 0;
  for (float i = 0; i < 16; ++i) {
    q.pop(out);
    EXPECT_EQ(out, i);
  }
  EXPECT_EQ(q.num_elements(), 0);
}

TEST(SPSCQueue, WrapAround) {
  SPSCQueue<float, 1024> q;
  float out = 0;

  // Enough pushes/pops to wrap the index arithmetic past capacity.
  constexpr std::size_t iterations = 3000;
  for (std::size_t i = 0; i < iterations; ++i) {
    q.push(static_cast<float>(i));
    q.pop(out);
    EXPECT_EQ(out, static_cast<float>(i));
  }
  EXPECT_EQ(q.num_elements(), 0);
}

TEST(SPSCQueue, ConcurrentProducerConsumer) {
  SPSCQueue<float, 1024> q;
  constexpr std::size_t total = 10000;

  std::thread producer([&q] {
    for (std::size_t i = 0; i < total; ++i) {
      q.push(static_cast<float>(i));
    }
  });

  std::vector<float> consumed;
  consumed.reserve(total);
  std::thread consumer([&q, &consumed] {
    for (std::size_t i = 0; i < total; ++i) {
      float out;
      q.pop(out);
      consumed.push_back(out);
    }
  });

  producer.join();
  consumer.join();

  ASSERT_EQ(consumed.size(), total);
  for (std::size_t i = 0; i < total; ++i) {
    ASSERT_EQ(consumed[i], static_cast<float>(i));
  }
}
