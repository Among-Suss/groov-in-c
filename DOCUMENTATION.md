# Groov-in-C Documentation

## Developer Environment Variables
| Name    | Description | Type |
| ----    | ----------- | ---- |
| LOG_LEVEL | Set logging level: ```0=LOG_TRACE 1=LOG_DEBUG 2=LOG_INFO 3=LOG_WARN 4=LOG_ERROR 5=LOG_FATAL```| int |
| DEBUG   | Enable debug commands | bool |

## Debug Commands

### -log
Sends the log file. 

**Warning:** Depending on your log level, this file may contain sensitive information such as bot tokens, so be mindful when using this command in a public server. 

## Code Structure

| File | Description |
| ---- | ----------- |
| test2 | Main file. Initializes all commands |
| discord | Discord  functions |
| sbuf | Linked-list library |
| media | Helper functions for playing media |
| litesocket/* | Helper functions for websocket connections |
| pyscripts/* | Python scripts for web scraping |

### External libraries
| File | Description |
| ---- | ----------- |
| strmap | ANSI C hash table for strings |
| cJSON | JSON parser in C |
| log | A simple logging library implemented in C99 |

## Logging

We use [rxi/log.c](https://github.com/rxi/log.c) for logging.

For the sake of migration, `fprintf` and `printf` are set as macros int `utils.h` to automatically call the corresponding log.c function. Minor changes are made to the library to automatically ignore newlines when using the macro function.