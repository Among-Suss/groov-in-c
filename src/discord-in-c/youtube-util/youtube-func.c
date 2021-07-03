#include "youtube-func.h"

/**
 * Sends a GET request to youtube with the encoded searchToken append to the
 * url, and parses the returning data with getVideoLinkFromHTML()
 * @param searchToken String to search for in youtube
 * @param linkToken Output link token
 */
void searchYoutubeForLink(char *searchToken, char *linkToken) {
  char url[46 + SEARCH_TOKEN_BUFFER] = SEARCH_URL;

  // GET Request https://curl.se/libcurl/c/getinmemory.html
  CURL *curl_handle;
  CURLcode res;

  struct MemoryStruct chunk;

  chunk.memory = malloc(1); /* will be grown as needed by the realloc above */
  chunk.size = 0;           /* no data at this point */

  curl_global_init(CURL_GLOBAL_ALL);

  /* init the curl session */
  curl_handle = curl_easy_init();

  // Encode query in case it uses non-English characters
  char *curl_encodedToken =
      curl_easy_escape(curl_handle, searchToken, strlen(searchToken));

  strcat(url, curl_encodedToken);

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
    getVideoLinkFromHTML(chunk.memory, linkToken);
  }

  /* cleanup curl stuff */
  curl_easy_cleanup(curl_handle);

  curl_free(curl_encodedToken);

  if (chunk.memory)
    free(chunk.memory);

  /* we're done with libcurl, so clean it up */
  curl_global_cleanup();
}

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

int main(int argc, char *argv[]) {
  char linkToken[YOUTUBE_TOKEN_SIZE];

  if (argc > 1) {
    searchYoutubeForLink(argv[1], linkToken);

    printf("%s%s\n", VIDEO_URL, linkToken);
  }
}