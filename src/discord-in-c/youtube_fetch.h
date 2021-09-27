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

typedef void (*insert_partial_ytp_callback)(void *media, char *id,
                                                       char *title,
                                                       char *duration,
                                                       int length);

/* ----------------------------- Main functions ----------------------------- */

/**
 * Fetches playlist and inserts into media list through callback.
 * @param url Video URL
 * @param start Start index
 * @param media Media object for callback
 * @param callback Insert partial youtube page callback
 * @param title Return pointer for playlist title. Must be freed
 * @return Error code
 */
int fetch_playlist(char *url, int start, void *media,
                   insert_partial_ytp_callback callback, char *title);

/**
 * Fetches description and escapes newlines and &
 * @param url Video URL
 * @param description Double pointer for return description. Must be freed
 */
int fetch_description(char *url, char **description);