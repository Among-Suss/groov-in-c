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
  int h, m, s;
  if (strlen(time) <= 5) {
    sscanf(time, "%d:%d", &m, &s);
    return m * 60 + s;
  } else {
    sscanf(time, "%d:%d:%d", &h, &m, &s);
    return h * 3600 + m * 60 + s;
  }

  return 0;
}

/* -------------------------------------------------------------------------- */
/*                                 FETCH DATA                                 */
/* -------------------------------------------------------------------------- */

/* ----------------------------- FETCH PLAYLIST ----------------------------- */

// Video playlist (where the link also contains a video) json macros
#define VIDEO_JSON_KEYS_LEN 5
#define VIDEO_JSON_KEYS                                                        \
  {                                                                            \
    "contents", "twoColumnWatchNextResults", "playlist", "playlist",           \
        "contents"                                                             \
  }
#define VIDEO_JSON_DATA_KEY "playlistPanelVideoRenderer"

// Playlist page json macros
#define PAGE_JSON_KEYS_LEN 14
#define PAGE_JSON_KEYS                                                         \
  {                                                                            \
    "contents", "twoColumnBrowseResultsRenderer", "tabs", {'\0'},              \
        "tabRenderer", "content", "sectionListRenderer", "contents", {'\0'},   \
        "itemSectionRenderer", "contents", {'\0'},                             \
        "playlistVideoListRenderer", "contents"                                \
  }
#define PAGE_JSON_DATA_KEY "playlistVideoRenderer"

// Error codes
#define FETCH_ERR 1
#define TRIM_ERR 2
#define LEN_ERR 3
#define KEY_ERR 4 // Happens with invalid playlist IDs

int fetch_playlist(char *url, int start, void *media,
                   void (*insert_partial_ytp_callback)(void *media, char *id,
                                                       char *title,
                                                       char *duration,
                                                       int length)) {

  // Fetch html
  char *html;
  int fetch_err = fetch_get(url, &html);
  if (fetch_err) {
    return FETCH_ERR;
  }

  // Search for data json and trim
  int trim_err = trim_between(html, "ytInitialData = ", ";</script>");
  if (trim_err) {
    free(html);
    return TRIM_ERR;
  }

  // Check if playlist page or video playlist
  int playlist_page = strstr(url, "/playlist") != NULL;

  // Get inner video list
  cJSON *video_json_list = cJSON_Parse(html);
  cJSON *inner_json = video_json_list;

  // Navigate into inner json
  if (playlist_page) {
    char keys[PAGE_JSON_KEYS_LEN][50] = PAGE_JSON_KEYS;

    for (int i = 0; i < PAGE_JSON_KEYS_LEN; i++) {
      if (!keys[i][0]) {
        inner_json = cJSON_GetArrayItem(inner_json, 0);
      } else {
        inner_json = cJSON_GetObjectItem(inner_json, keys[i]);
      }
    }
  } else {
    char keys[VIDEO_JSON_KEYS_LEN][50] = VIDEO_JSON_KEYS;

    for (int i = 0; i < VIDEO_JSON_KEYS_LEN; i++) {
      inner_json = cJSON_GetObjectItem(inner_json, keys[i]);
    }

    if (!inner_json) {
      cJSON_Delete(video_json_list);
      free(html);
      return KEY_ERR;
    }
  }

  int len = cJSON_GetArraySize(inner_json);

  // Insert each video
  for (int i = start; i < len; i++) {
    cJSON *data = cJSON_GetArrayItem(inner_json, i);

    // Skip if the last video json is a continuation link rather than a video
    if (cJSON_HasObjectItem(data, "continuationItemRenderer")) {
      continue;
    }

    cJSON *video = cJSON_GetObjectItem(
        data, playlist_page ? PAGE_JSON_DATA_KEY : VIDEO_JSON_DATA_KEY);

    // Get key values
    char *id = cJSON_GetStringValue(cJSON_GetObjectItem(video, "videoId"));

    char *title;
    if (playlist_page) {
      title = cJSON_GetStringValue(cJSON_GetObjectItem(
          cJSON_GetArrayItem(
              cJSON_GetObjectItem(cJSON_GetObjectItem(video, "title"), "runs"),
              0),
          "text"));
    } else {
      title = cJSON_GetStringValue(cJSON_GetObjectItem(
          cJSON_GetObjectItem(video, "title"), "simpleText"));
    }

    char *duration = cJSON_GetStringValue(cJSON_GetObjectItem(
        cJSON_GetObjectItem(video, "lengthText"), "simpleText"));

    if (!duration) {
      cJSON_Delete(video_json_list);
      free(html);

      return LEN_ERR;
    }

    int time = parse_time(duration);

    insert_partial_ytp_callback(media, id, title, duration, time);
  }

  cJSON_Delete(video_json_list);
  free(html);

  return 0;
}