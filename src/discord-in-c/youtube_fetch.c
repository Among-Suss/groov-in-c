#include "youtube_fetch.h"

/* -------------------------------------------------------------------------- */
/*                               LIBCURL HELPER                               */
/* -------------------------------------------------------------------------- */

// Lib curl callback
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb,
                                  void *userp) {
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;

  mem->memory = (char *)realloc(mem->memory, mem->size + realsize + 1);
  if (mem->memory == NULL) {
    /* out of memory! */
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }

  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

/**
 * @param url url to fetch
 * @param raw raw output html. Must be freed ONLY IF error code returns 0
 * @return error
 */
int fetch_get(char *url, char **raw) {
  // GET Request https://curl.se/libcurl/c/getinmemory.html
  CURL *curl_handle;
  CURLcode res;

  struct MemoryStruct chunk;

  chunk.memory =
      (char *)malloc(1); /* will be grown as needed by the realloc above */
  chunk.size = 0;        /* no data at this point */

  curl_global_init(CURL_GLOBAL_ALL);

  /* init the curl session */
  curl_handle = curl_easy_init();

  /* specify URL to get */
  curl_easy_setopt(curl_handle, CURLOPT_URL, url);

  /* send all data to this function  */
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

  /* we pass our 'chunk' struct to the callback function */
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);

  /* some servers don't like requests that are made without a user-agent
     field, so we provide one */
  curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

  /* get it! */
  res = curl_easy_perform(curl_handle);

  /* check for errors */
  if (res != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(res));
  } else {
    *raw = chunk.memory;
  }

  /* cleanup curl stuff */
  curl_easy_cleanup(curl_handle);

  /* we're done with libcurl, so clean it up */
  curl_global_cleanup();

  return res;
}

/* -------------------------------------------------------------------------- */
/*                                STRING HELPER                               */
/* -------------------------------------------------------------------------- */

/**
 * Trims a string between two delimiters
 * @return err
 */
int trim_between(char *restrict text, char const *start, char const *end) {
  char *p1, *p2;

  p1 = strstr(text, start);
  if (p1) {
    p2 = strstr(p1, end);

    int length = (int)(p2 - p1 - strlen(start));
    if (p2) {
      snprintf(text, length + 1, "%.*s", length, p1 + strlen(start));
      return 0;
    }
  }
  return 1;
}

int parse_time(char *const time) {
  int h = -1, m = -1, s = -1;
  if (strlen(time) <= 5) {
    sscanf(time, "%d:%2d", &m, &s);
    h = 0;
  } else {
    sscanf(time, "%d:%d:%2d", &h, &m, &s);
  }
  if (h == -1 || m == -1 || s == -1 || !isdigit(time[strlen(time) - 1])) {
    return -1;
  }

  return h * 3600 + m * 60 + s;
}

int parse_duration(double length, char *duration) {
  int secs = ((int)length / 1000) % 60;
  int mins = ((int)(length / 1000 / 60) % 60);
  int hrs = ((int)(length / 1000 / 3600));

  if (hrs != 0) {
    snprintf(duration, 16, "%d:%02d:%02d", hrs, mins, secs);
  } else {
    snprintf(duration, 16, "%2d:%02d", mins, secs);
  }

  return 0;
}

int parse_description_timestamps(char *description, cJSON *timestamp_arr) {
  char *saveptr1;

  while (1) {
    char *line = strtok_r(description, "\n", &saveptr1);

    if (line == NULL)
      break;

    char label_text[1024];

    strncpy(label_text, line, 1023);

    regex_t regex;
    regmatch_t pmatch[1];

    if (regcomp(&regex, "([0-9]*:)?[0-9]?[0-9]:[0-9][0-9]", REG_EXTENDED)) {
      fprintf(stderr, "Regex comp error...?");
      regfree(&regex);
      break;
    }

    if (!regexec(&regex, line, 1, pmatch, 0)) {
      char timestamp_word[50];

      snprintf(timestamp_word, 50, "%.*s", pmatch[0].rm_eo - pmatch[0].rm_so,
               line + pmatch[0].rm_so);

      if (parse_time(timestamp_word) != -1) {
        cJSON *item = cJSON_CreateObject();

        cJSON_AddItemToObject(item, "label", cJSON_CreateString(label_text));
        cJSON_AddItemToObject(item, "timestamp",
                              cJSON_CreateNumber(parse_time(timestamp_word)));

        cJSON_AddItemToArray(timestamp_arr, item);
      }
    }

    regfree(&regex);

    description = NULL;
  }

  return 0;
}
