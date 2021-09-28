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
    "contents", "twoColumnBrowseResultsRenderer", "tabs", "\0", "tabRenderer", \
        "content", "sectionListRenderer", "contents", "\0",                    \
        "itemSectionRenderer", "contents", "\0", "playlistVideoListRenderer",  \
        "contents"                                                             \
  }
#define PAGE_JSON_DATA_KEY "playlistVideoRenderer"

// Error codes
#define FETCH_ERR 1
#define TRIM_ERR 2
#define LEN_ERR 3
#define KEY_ERR 4 // Happens with invalid playlist IDs

int fetch_playlist(char *url, int start, void *media,
                   insert_partial_ytp_callback_f callback, char *title) {

  // Fetch html
  char *html = NULL;
  cJSON *video_json_list = NULL;
  int ret = 0;

  int fetch_err = fetch_get(url, &html);
  if (fetch_err) {
    ret = FETCH_ERR;
    goto PLAYLIST_FETCH_CLEANUP;
  }

  // Search for data json and trim
  int trim_err = trim_between(html, "ytInitialData = ", ";</script>");
  if (trim_err) {
    ret = TRIM_ERR;
    goto PLAYLIST_FETCH_CLEANUP;
  }

  // Check if playlist page or video playlist
  int playlist_page = strstr(url, "/playlist") != NULL;

  // Get inner video list
  video_json_list = cJSON_Parse(html);
  cJSON *inner_json = video_json_list;

  char *playlist_title = NULL;

  // Navigate into inner json
  if (playlist_page) {
    char keys[PAGE_JSON_KEYS_LEN][50] = PAGE_JSON_KEYS;

    playlist_title = cJSON_GetStringValue(cJSON_GetObjectItem(
        cJSON_GetObjectItem(cJSON_GetObjectItem(inner_json, "metadata"),
                            "playlistMetadataRenderer"),
        "title"));

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

      if (strncmp(keys[i], "playlist", 8) == 0 && !playlist_title) {
        playlist_title = cJSON_GetStringValue(cJSON_GetObjectItem(
            cJSON_GetObjectItem(inner_json, "playlist"), "title"));
      }
    }

    // Invalid playlist IDs will be missing keys here
    if (!inner_json) {
      ret = KEY_ERR;
      goto PLAYLIST_FETCH_CLEANUP;
    }
  }

  snprintf(title, strlen(playlist_title) + 1, "%s", playlist_title);

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

    char *song_title;
    if (playlist_page) {
      song_title = cJSON_GetStringValue(cJSON_GetObjectItem(
          cJSON_GetArrayItem(
              cJSON_GetObjectItem(cJSON_GetObjectItem(video, "title"), "runs"),
              0),
          "text"));
    } else {
      song_title = cJSON_GetStringValue(cJSON_GetObjectItem(
          cJSON_GetObjectItem(video, "title"), "simpleText"));
    }

    char *duration = cJSON_GetStringValue(cJSON_GetObjectItem(
        cJSON_GetObjectItem(video, "lengthText"), "simpleText"));

    if (!duration) {
      ret = LEN_ERR;
      goto PLAYLIST_FETCH_CLEANUP;
    }

    int time = parse_time(duration);

    callback(media, id, song_title, duration, time);
  }

PLAYLIST_FETCH_CLEANUP:
  if (video_json_list != NULL)
    cJSON_Delete(video_json_list);
  if (html != NULL)
    free(html);

  return ret;
}

int parse_description_timestamps(char *description,
                                 insert_timestamp_callback insert_timestamp) {
  char *saveptr1, *saveptr2;

  while (1) {
    char *line = strtok_r(description, "\n", &saveptr1);

    if (line == NULL)
      break;

    char line_buffer[1024];

    strncpy(line_buffer, line, 1023);

    char *word = strtok_r(line, " ", &saveptr2);

    if (parse_time(word) != -1) {
      insert_timestamp(parse_time(word), line_buffer);
    }

    description = NULL;
  }

  return 0;
}