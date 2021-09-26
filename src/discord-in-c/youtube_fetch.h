#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "cJSON.h"
#include "media.h"

/* -------------------------------- Lib curl -------------------------------- */
struct MemoryStruct {
  char *memory;
  size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb,
                                  void *userp);

/* --------------------------------- Helpers -------------------------------- */
int fetch_get(char *url, char **raw);

int trim_between(char *text, char const *start, char const *end);


/* ----------------------------- Main functions ----------------------------- */

/**
 * Fetches playlist and inserts into media list through callback
 * @param url
 * @param media Media object for callback (Used void pointer to avoid requiring dependency, is this a good idea?)
 * @param insert_partial_ytp_callback
 */
int fetch_playlist(char *url, int start, void *media,
                   void (*insert_partial_ytp_callback)(void *media, char *id,
                                                       char *title,
                                                       char *duration,
                                                       int length));
