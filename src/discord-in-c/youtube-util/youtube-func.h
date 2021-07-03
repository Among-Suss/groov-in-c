#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "youtube-util.h"

#define SEARCH_URL "https://www.youtube.com/results?search_query="
#define VIDEO_URL "https://www.youtube.com/watch?v="
#define SEARCH_TOKEN_BUFFER 200
#define YOUTUBE_TOKEN_SIZE 15

/**
 * Gets the youtube video token of the first search result of searchToken
 * @param searchToken String to search for in youtube
 * @param linkToken Output link token
 */
void searchYoutubeForLink(char *searchToken, char *linkToken);

// Lib curl
struct MemoryStruct {
  char *memory;
  size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb,
                                  void *userp);
