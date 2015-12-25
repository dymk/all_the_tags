#include "gtest/gtest.h"
#include "context.h"
#include "test_helper.h"

TEST(ContextTest, DupTagCreation) {
  Context c;
  ASSERT_TRUE(c.new_tag(1));
  ASSERT_FALSE(c.new_tag(1));
  ASSERT_TRUE(c.new_tag(2));
}
