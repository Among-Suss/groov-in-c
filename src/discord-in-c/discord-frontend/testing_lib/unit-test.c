#include "terminal_styles.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SUITE_NAME_LENGTH 30
#define TEST_NAME_LENGTH 30

typedef struct {
  char name[SUITE_NAME_LENGTH];
  int test_num;
  int passed;
  int failed;
} test_suite;

test_suite *startTests(char *name) {
  test_suite *suite = malloc(sizeof(test_suite));

  strncpy(suite->name, name, SUITE_NAME_LENGTH);
  suite->test_num = 1;
  suite->passed = 0;
  suite->failed = 0;

  printf(BLD "\n<%s> \n\n" RESET, name);

  return suite;
}

void endTests(test_suite *suite) {
  printf("\n> Tests: ");
  if (suite->passed > 0) {
    printf(GRN "%d passed, " RESET, suite->passed);
  }
  if (suite->failed > 0) {
    printf(RED "%d failed, " RESET, suite->failed);
  }

  printf(FNT "%d total\n\n" RESET, suite->test_num - 1);

  free(suite);
}

void checkResults(test_suite *suite, char *message, int passed) {
  if (passed) {
    suite->passed++;
    printf("%s%s\tPassed" RESET ": ", BLK, GRN_BG);
  } else {
    suite->failed++;
    printf("%s%s\tFailed" RESET ": ", BLK, RED_BG);
  }

  printf("(%d) \"%s\"\n", suite->test_num++, message);
}

int assert(test_suite *suite, char *message, int passed) {

  checkResults(suite, message, passed);

  return passed;
}
