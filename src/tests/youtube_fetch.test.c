#include "test.h"
#include "../discord-in-c/sbuf.h"
#include "../discord-in-c/sbuf.structs.h"
#include "../discord-in-c/media.h"
#include "../discord-in-c/media.structs.h"

#define URL                                                                    \
  "https://www.youtube.com/"                                                   \
  "watch?v=dyKdLLQP5PI&list=PLlJZn_WcZ1FJoxyPD2XU2Zz0lPm5h4ivi&index=1"
#define URL_PAGE                                                               \
  "https://www.youtube.com/playlist?list=PLlJZn_WcZ1FJhllZhSUSoZUurIr04i-TL"
#define URL_BAD                                                                \
  "https://www.youtube.com/watch?v=01skyPMeeoc&list=PL8D03A4C0FFBBE36"

#define MOCK_TS_DESC_START                                                     \
  "Lorem ipsum\n\n0:00 - Test1\n12:12 - Test2\n1:12:21 - Test3\n\nEnd\n"

#define MOCK_TS_DESC_MID                                                       \
  "Lorem ipsum\n\ntest - 0:00- Test1\ntest - 12:12 - Test2\ntest - 1:12:21 "   \
  "- Test3\n\nEnd\n"

#define MOCK_TS_DESC_END                                                       \
  "Lorem ipsum\n\nTest1 -0:00\nTest2 - 12:12\nTest3- 122:12:12\n\nEnd\n"

#define RETRIES 10

void mock_insert(void *media, char *id, char *title, char *duration,
                 int length) {
}

int timestamp_count = 0;

INIT_SUITE

int main(int argc, char **argv) {
  printf("Suite: %s\n", argv[0]);

  int retries = 0;

  char title[1024];

  media_player_t *media = malloc(sizeof(media_player_t));

  sbuf_init(&(media->song_queue));

  int url_err = 1;
  for (int i = 0; i < RETRIES; i++) {
    url_err = fetch_youtube_playlist(URL, 0, media, title, sizeof(title));
    if (url_err == 0) {
      retries = i;
      break;
    }
  }

  assert(0, url_err, "Regular URL should succeed: %d attempt(s)", retries + 1);
  assert_str("weird jap music", title, "Regular URL should have correct title");

  int url_page_err = 1;
  for (int i = 0; i < RETRIES; i++) {
    url_page_err =
        fetch_youtube_playlist(URL_PAGE, 0, media, title, sizeof(title));
    if (url_page_err == 0) {
      retries = i;
      break;
    }
  }
  assert(0, url_page_err, "Page URL should succeed: %d attempt(s)",
         retries + 1);
  assert_str("Jazz", title, "Page URL should have correct title");

  int bad_url_err =
      fetch_youtube_playlist(URL_BAD, 0, media, title, sizeof(title));
  assert(4, bad_url_err, "Bad URL should fail");

  // Timestamps at start
  char desc[6000] = MOCK_TS_DESC_START;

  cJSON *ts_arr = cJSON_CreateArray();
  parse_description_timestamps(desc, ts_arr);
  assert(3, cJSON_GetArraySize(ts_arr),
         "Parse timestamp should find 3 timestamps at the start");

  cJSON_Delete(ts_arr);

  // Timestamps at middle

  char desc_mid[6000] = MOCK_TS_DESC_MID;

  ts_arr = cJSON_CreateArray();
  parse_description_timestamps(desc_mid, ts_arr);
  assert(3, cJSON_GetArraySize(ts_arr),
         "Parse timestamp should find 3 timestamps in the middle");

  cJSON_Delete(ts_arr);

  // Timestamps at end

  char desc_end[6000] = MOCK_TS_DESC_END;

  ts_arr = cJSON_CreateArray();
  parse_description_timestamps(desc_end, ts_arr);
  assert(3, cJSON_GetArraySize(ts_arr),
         "Parse timestamp should find 3 timestamps at the end");

  cJSON_Delete(ts_arr);

  END_SUITE;
  END_SUITE
}