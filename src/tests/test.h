#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define RESET "\x1b[0m"

#define INIT_SUITE                                                             \
  int passed = 1;                                                              \
  int tests = 0;                                                               \
                                                                               \
  int assert(char *message, int expected, int actual) {                        \
    int pass = expected == actual;                                             \
                                                                               \
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
  }

#define END_SUITE                                                              \
  if (passed) {                                                                \
    puts("Tests passed!");                                                     \
    return 0;                                                                  \
  } else {                                                                     \
    puts("Tests failed");                                                      \
    return 1;                                                                  \
  }
