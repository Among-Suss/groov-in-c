#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>
#include <curl/curl.h>
#include "cJSON.h"
#include "log.h"

/* -------------------------------- Lib curl -------------------------------- */
struct MemoryStruct {
  char *memory;
  size_t size;
};

/* --------------------------------- Helpers -------------------------------- */
int fetch_get(char *url, char **raw);

int trim_between(char *text, char const *start, char const *end);

/* ----------------------------- Main functions ----------------------------- */

/**
 * Fetches description and escapes newlines and &
 * @param url Video URL
 * @param description Pointer to buffer where the description will be stored.
 * Should contain atleast 6000 characters
 */
int fetch_description_youtube_dl(char *url, char *description);

/**
 * Parses a non-newline escaped description and returns a JSON list of
 * timestamps and labels
 * @param description
 * @param timestamps_arr An empty cJSON array. Returns a list of objects in the
 * format [{"timestamp": int, "label": str}]. Must be freed with cJSON_Delete
 */
int parse_description_timestamps(char *description, cJSON *timestamps_arr);

int parse_time(char *const time);

int parse_duration(double length, char *duration);