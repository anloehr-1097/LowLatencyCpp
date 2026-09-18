#include <Arena.h>
#include <gtest/gtest.h>

TEST(ArenaSimpleUse, ArenaCreate) {
  {
    auto ar = ArenaBumpAllocator<1024>();
    EXPECT_NE(ar.mem, nullptr);
  }
  auto ar = ArenaBumpAllocator<1024>();
  EXPECT_NE(ar.mem, nullptr);
}

TEST(ArenaSimpleUse, ArenaAllocate) {
  auto ar = ArenaBumpAllocator<1024>();
  EXPECT_NE(ar.mem, nullptr);
  auto char_ptr = static_cast<char *>(ar.allocate(10, alignof(char)));
  EXPECT_NE(char_ptr, nullptr);
}
