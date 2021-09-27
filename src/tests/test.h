#include <stdarg.h>

#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define RESET "\x1b[0m"

#define FORMAT_MESSAGE                                                         \
  char message[1024];                                                          \
  va_list args;                                                                \
  va_start(args, fmt);                                                         \
  vsprintf(message, fmt, args);                                                \
  va_end(args);

#define INIT_SUITE                                                             \
  int passed = 1;                                                              \
  int tests = 0;                                                               \
                                                                               \
  int assert(int expected, int actual, const char *fmt, ...) {                 \
    int pass = expected == actual;                                             \
    FORMAT_MESSAGE                                                             \
    if (pass) {                                                                \
      printf("\t" GREEN "PASSED: " RESET "%s\n", message);                     \
    } else {                                                                   \
      printf("\t" RED "FAILED: " RESET "%s\n", message);                       \
      printf("\t\t" RED " -Expected: %d, Actual: %d\n" RESET, expected,        \
             actual);                                                          \
    }                                                                          \
                                                                               \
    passed = passed && pass;                                                   \
                                                                               \
    return pass;                                                               \
  }                                                                            \
  int assert_str(char *expected, char *actual, const char *fmt, ...) {         \
    int pass = strcmp(expected, actual) == 0;                                  \
    FORMAT_MESSAGE                                                             \
    if (pass) {                                                                \
      printf("\t" GREEN "PASSED: " RESET "%s\n", message);                     \
    } else {                                                                   \
      printf("\t" RED "FAILED: " RESET "%s\n", message);                       \
      printf("\t\t" RED " -Expected: %s, Actual: %s\n" RESET, expected,        \
             actual);                                                          \
    }                                                                          \
                                                                               \
    passed = passed && pass;                                                   \
                                                                               \
    return pass;                                                               \
  }                                                                            \
                                                                               \
  int assert_ptr(void *expected, void *actual, const char *fmt, ...) {         \
    int pass = expected == actual;                                             \
    FORMAT_MESSAGE                                                             \
    if (pass) {                                                                \
      printf("\t" GREEN "PASSED: " RESET "%s\n", message);                     \
    } else {                                                                   \
      printf("\t" RED "FAILED: " RESET "%s\n", message);                       \
      printf("\t\t" RED " -Expected: %p, Actual: %p\n" RESET, expected,        \
             actual);                                                          \
    }                                                                          \
                                                                               \
    passed = passed && pass;                                                   \
                                                                               \
    return pass;                                                               \
  }

#define END_SUITE                                                              \
  if (passed) {                                                                \
    puts("Tests passed!");                                                     \
    return 0;                                                                  \
  } else {                                                                     \
    puts("Tests failed");                                                      \
    return 1;                                                                  \
  }
