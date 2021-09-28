#include "log.h"

// Logging
#define printf(message, args...) log_print_out(message, ##args)
#define fprintf(channel, message, args...)                                     \
  channel == stdout ? log_print_out(message, ##args)                           \
                    : log_print_err(message, ##args)

// Env
#define DEBUG getenv("DEBUG")