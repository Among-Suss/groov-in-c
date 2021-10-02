#include "log.h"

// Logging
#define printf(...) log_print_out(__VA_ARGS__)
#define fprintf(channel, ...)                                         \
  channel == stdout ? log_print_out(__VA_ARGS__)                      \
                    : log_print_err(__VA_ARGS__)

// Env
#define DEBUG getenv("DEBUG") == NULL