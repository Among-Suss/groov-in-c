#include "log.h"

// Logging
#ifdef LOGGING
#undef LOGGING
#define LOGGING 1
#define printf(...) log_print_out(__VA_ARGS__)
#define fprintf(channel, ...)                                                  \
  channel == stdout ? log_print_out(__VA_ARGS__) : log_print_err(__VA_ARGS__)
#else
// Reverse the macros to printf if logging is disabled
#define LOGGING 0
#define log_trace(...)                                                         \
  fprintf(stdout, __VA_ARGS__);                                                \
  fprintf(stdout, "\n")
#define log_debug(...)                                                         \
  fprintf(stdout, __VA_ARGS__);                                                \
  fprintf(stdout, "\n")
#define log_info(...)                                                          \
  fprintf(stdout, __VA_ARGS__);                                                \
  fprintf(stdout, "\n")
#define log_warn(...)                                                          \
  fprintf(stderr, __VA_ARGS__);                                                \
  fprintf(stderr, "\n")
#define log_error(...)                                                         \
  fprintf(stderr, __VA_ARGS__);                                                \
  fprintf(stderr, "\n")
#define log_fatal(...)                                                         \
  fprintf(stderr, __VA_ARGS__);                                                \
  fprintf(stderr, "\n")
#endif
