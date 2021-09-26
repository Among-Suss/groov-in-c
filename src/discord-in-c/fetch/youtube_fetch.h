#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "../cJSON.h"

/* -------------------------------- Lib curl -------------------------------- */
struct MemoryStruct {
  char *memory;
  size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb,
                                  void *userp);


/* --------------------------------- Helpers -------------------------------- */
int fetch_get(char *url, char **raw);

int trim_between(char *restrict text, char const *start, char const *end);

int fetch_playlist(char *url);