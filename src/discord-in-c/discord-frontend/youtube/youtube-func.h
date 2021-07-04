#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <regex.h>
#include "../../sbuf.c"


#define SEARCH_URL "https://www.youtube.com/results?search_query="
#define VIDEO_URL "https://www.youtube.com/watch?v="
#define SEARCH_TOKEN_BUFFER 200
#define YOUTUBE_TOKEN_SIZE 20 // Average seems to be 11-ish, so slightly above just to be safe
#define YOUTUBE_VIDEO_URL_SIZE 52 // Again, slightly higher just to be safe

/**
 * Gets the youtube video token of the first search result of searchToken
 * @param searchToken String to search for in youtube
 * @param linkToken Output link token
 */
void search_youtube_for_link_token(char *searchToken, char *linkToken);

/**
 * Gets the youtube video url of the first search result
 * @param searchToken
 * @param url Url string. Size must be YOUTUBE_VIDEO_URL_SIZE
 */
void search_youtube_for_link(char *searchToken, char *url);


/* -------------------------------- Lib curl -------------------------------- */
struct MemoryStruct {
  char *memory;
  size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb,
                                  void *userp);


/* ---------------------------------- Utils --------------------------------- */

#define HTML_BUFFER 4069

/**
 * Parses raw HTML string of a youtube search page for the first search result
 * @param html HTML string to parse
 * @param outputLinkToken Char pointer to the string to store the output
 */
void getVideoLinkFromHTML(char *html, char *outputLinkToken);

#define DESC_LENGTH 150
#define TIMESTAMP_REGEX "[0-9]*:[0-9][0-9]\\(:[0-9][0-9]\\)*"
#define TIMESTAMP_LENGTH 8

void getTimeStampsFromHTML(char *html, struct sbuf_t *timestampList);