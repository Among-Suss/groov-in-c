#include "log.h"

#define printf(message, args...) log_info(message, ##args)
#define fprintf(channel, message, args...) log_info(message, ##args)