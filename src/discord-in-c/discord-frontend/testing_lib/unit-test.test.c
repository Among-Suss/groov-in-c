#include "unit-test.c"

int main(void) {
  test_suite *tests = startTests("Unit tests");

  assert(tests, "True should equal true", 1 == 1);

  assert(tests, "False should not equal true", 1 == 0);

  endTests(tests);
}