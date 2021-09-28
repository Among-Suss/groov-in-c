#include "test.h"
#include "../discord-in-c/youtube_fetch.h"

#define URL                                                                    \
  "https://www.youtube.com/"                                                   \
  "watch?v=dyKdLLQP5PI&list=PLlJZn_WcZ1FJoxyPD2XU2Zz0lPm5h4ivi&index=1"
#define URL_PAGE                                                               \
  "https://www.youtube.com/playlist?list=PLlJZn_WcZ1FJhllZhSUSoZUurIr04i-TL"
#define URL_BAD                                                                \
  "https://www.youtube.com/watch?v=01skyPMeeoc&list=PL8D03A4C0FFBBE36"

#define MOCK_DESCRIPTION                                                       \
  "Lorem ipsum\n\n0:00 - Test1\n12:12 - Test2\n1:12:21 - Test3\n\nEnd\n"

#define RETRIES 10

void mock_insert(void *media, char *id, char *title, char *duration,
                 int length) {
}

int timestamp_count = 0;
void mock_ts_insert(int length, char *label) {
  //printf("[%d] %s\n", length, label);
  timestamp_count++;
}

INIT_SUITE

int main(int argc, char **argv) {
  printf("Suite: %s\n", argv[0]);

  int retries = 0;

  char title[1024];

  int url_err = 1;
  for (int i = 0; i < RETRIES; i++) {
    url_err = fetch_playlist(URL, 0, NULL, mock_insert, title);
    if (url_err == 0) {
      retries = i;
      break;
    }
  }

  assert(0, url_err, "Regular URL should succeed: %d attempt(s)", retries + 1);
  assert_str("weird jap music", title, "Regular URL should have correct title");

  int url_page_err = 1;
  for (int i = 0; i < RETRIES; i++) {
    url_page_err = fetch_playlist(URL_PAGE, 0, NULL, mock_insert, title);
    if (url_page_err == 0) {
      retries = i;
      break;
    }
  }
  assert(0, url_page_err, "Page URL should succeed: %d attempt(s)",
         retries + 1);
  assert_str("Jazz", title, "Page URL should have correct title");

  int bad_url_err = fetch_playlist(URL_BAD, 0, NULL, mock_insert, title);
  assert(4, bad_url_err, "Bad URL should fail");

  char desc[6000] = MOCK_DESCRIPTION;
  parse_description_timestamps(desc, mock_ts_insert);
  assert(3, timestamp_count, "Parse timestamp should find 3 timestamps");


  END_SUITE
}