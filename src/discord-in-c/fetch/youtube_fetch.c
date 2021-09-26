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
      snprintf(text, 600, "%.*s", length, p1 + strlen(start));
      return 0;
    }
  }
  return 1;
}

/* -------------------------------------------------------------------------- */
/*                                 FETCH DATA                                 */
/* -------------------------------------------------------------------------- */

//(media_player_t *media, char *id, char *title, char *duration, int length)

int fetch_playlist(char *url,
                   void (*insert_partial_ytp_callback)(char *id, char *title,
                                                       char *duration,
                                                       int length)) {

  char *html;
  int fetch_err = fetch_get(url, &html);


  if (fetch_err) {
    return 1;
  }

  int trim_err = trim_between(html, "ytInitialData = ", ";</script>");

  if (trim_err) {
    return 1;
  }
  printf("%s", html);

  free(html);
}

void mock(char *id, char *title, char *duration, int length) {
  printf("ID: %s, Title: %s, Duration: %s, Length: %d", id, title, duration,
         length);
}

#define URL                                                                    \
  "https://www.youtube.com/playlist?list=PLlJZn_WcZ1FJhllZhSUSoZUurIr04i-TL"

int main() {
  fetch_playlist(URL, mock);
}