#include "test.h"
#include "../discord-in-c/youtube_fetch.h"

#define URL                                                                    \
  "https://www.youtube.com/"                                                   \
  "watch?v=dyKdLLQP5PI&list=PLlJZn_WcZ1FJoxyPD2XU2Zz0lPm5h4ivi&index=1"
#define URL_PAGE                                                               \
  "https://www.youtube.com/playlist?list=PLlJZn_WcZ1FJhllZhSUSoZUurIr04i-TL"
#define URL_BAD                                                                \
  "https://www.youtube.com/watch?v=01skyPMeeoc&list=PL8D03A4C0FFBBE36"

#define RETRIES 10

int count = 0;

void mock_insert(void *media, char *id, char *title, char *duration,
                 int length) {
  count++;
}

INIT_SUITE

int main(int argc, char **argv) {
  printf("Suite: %s\n", argv[0]);

  count = 0;
  int url = 1;

  for (int i = 0; i < RETRIES; i++) {
    url = fetch_playlist(URL, 0, NULL, mock_insert);
    if (url == 0) break;
  }

  assert("Regular URL should succeed", 0, url);
  assert("Regular URL should queue 200 songs", 200, count);

  count = 0;
  int url_page = 1;
  for (int i = 0; i < RETRIES; i++) {
    url_page = fetch_playlist(URL_PAGE, 0, NULL, mock_insert);
    if (url_page == 0)
      break;
  }
  assert("Page URL should succeed", 0, url_page);
  assert("Page URL should queue 100 songs", 100, count);

  int bad_url = fetch_playlist(URL_BAD, 0, NULL, mock_insert);
  assert("Bad URL should fail", 4, bad_url);

  END_SUITE
}