#include "log.h"

// Logging
#define printf(message, args...) log_print(message, ##args)
#define fprintf(channel, message, args...) log_print(message, ##args)

// Env
#define DEBUG getenv("DEBUG")