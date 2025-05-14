#include "doctest.h"

TEST_CASE("Example Test Case")
{
  int a = 1;
  int b = 2;
  CHECK(a + b == 3);
  CHECK_FALSE(a + b == 4);
}

TEST_CASE("Another Example Test Case")
{
  std::string s = "hello";
  CHECK(s.length() == 5);
  s += " world";
  CHECK(s == "hello world");
}