# Groov-in-C Documentation

## Developer Environment Variables
| Name    | Description | Type |
| ----    | ----------- | ---- |
| LOG_LEVEL | Set logging level: ```0=LOG_TRACE 1=LOG_DEBUG 2=LOG_INFO 3=LOG_WARN 4=LOG_ERROR 5=LOG_FATAL```| int |
| DEBUG   | Enable debug commands | bool |

## Debug Commands

 - log: Sends the log file. 
   - **Warning:** Depending on your log level, this file may contain sensitive information such as bot tokens, so be mindful when using this command in a public server. 

## Code Structure

| File | Description |
| ---- | ----------- |
| test2 | Main file. Initializes all commands |
| discord.c | Discord  functions |
| sbuf.c | Linked-list library |
| media.c | Helper functions for playing media |
| youtube_fetch.c | Youtube scraping library |
| litesocket/litesocket.c | Helper functions for websocket connections |
| utils.h | Contains macros for development |

### External libraries
| File | Description |
| ---- | ----------- |
| strmap.c | ANSI C hash table for strings |
| cJSON.c | JSON parser in C |
| log.c | A simple logging library implemented in C99 |

## Logging

We use [rxi/log.c](https://github.com/rxi/log.c) for logging.

For the sake of migration, `fprintf` and `printf` are set as macros int `utils.h` to automatically call the corresponding log.c function. Minor changes are made to the library to automatically ignore newlines when using the macro function.

## Testing

Basic unit tests are located in `src/tests`. These test are more to check whether or not the Youtube API has changed, as this will break playlists and a few other functions that rely on `youtube_fetch.c`. 

To manually run tests, run the following command in the tests directory.
```sh
make tests
``` 
or use the `-C` flag to run from root
```sh
make -C src/tests
```

To add more tests, use create a test file and add its build to the Makefile. You can also use [test.h](/src/tests/test.h), which is a tiny testing module that can be used for basic assertion tests.