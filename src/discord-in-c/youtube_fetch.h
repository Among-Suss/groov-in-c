#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <curl/curl.h>
#include "cJSON.h"

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

typedef void (*insert_timestamp_callback)(int time, char *text);

/* ----------------------------- Main functions ----------------------------- */

/**
 * @brief Fetches playlist and inserts into media list through callback.
 * Error codes:
 *  1: Curl error,
 *  2: Trim error,
 *  3: Json lengthText key error,
 *  4: Json playlist key error (likely due to invalid playlist ID)

 * @param url Video URL
 * @param start Start index
 * @param media Media object for callback
 * @param callback Insert partial youtube page callback
 * @param title Pointer to buffer where the title will be stored. Should contain atleast 200 characters
 * @return Error code
 */
int fetch_playlist(char *url, int start, void *media,
                   insert_partial_ytp_callback callback, char *title);

/**
 * Fetches description and escapes newlines and &
 * @param url Video URL
 * @param description Pointer to buffer where the description will be stored. Should
 * contain atleast 6000 characters
 */
int fetch_description_youtube_dl(char *url, char *description);

int parse_description_timestamps(char *description,
                                 insert_timestamp_callback insert_timesamp);