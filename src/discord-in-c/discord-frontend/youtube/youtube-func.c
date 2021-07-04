#include "youtube-func.h"

/* -------------------------------------------------------------------------- */
/*                     Functions for getting youtube data                     */
/* -------------------------------------------------------------------------- */

/**
 * Sends a GET request to youtube with the encoded searchToken append to the
 * url, and parses the returning data with getVideoLinkFromHTML()
 * @param searchToken String to search for in youtube
 * @param linkToken Output link token
 */
void search_youtube_for_link_token(char *searchToken, char *linkToken) {
  char url[46 + SEARCH_TOKEN_BUFFER] = SEARCH_URL;

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

/**
 * Wraps search_youtube_for_link_token to automatically prepend the youtube
 * watch url
 * @param searchToken
 * @param url
 */
void search_youtube_for_link(char *searchToken, char *url) {
  char token[YOUTUBE_TOKEN_SIZE];
  char youtubeUrl[YOUTUBE_VIDEO_URL_SIZE] = VIDEO_URL;

  search_youtube_for_link_token(searchToken, token);

  strcat(youtubeUrl, token);
  strcpy(url, youtubeUrl);
}

void youtube_get_timestamps(char *url) {
  FILE *fp;
  char cmd[YT_DL_DESC_MAX] = YT_DL_DESC_COMMAND;

  strcat(cmd, url);

  puts(cmd);

  fp = popen(cmd, "r");

  getVideoTimeStampsFromHTMLFile(fp);

  pclose(fp);
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

/* -------------------------------------------------------------------------- */
/*                                    Utils                                   */
/* -------------------------------------------------------------------------- */

/**
 * Searches through html for a match with "videoId", and retreives the string
 * following it
 * @param html HTML string to parse
 * @param outputLinkToken Char pointer to the string to store the output
 */
void getVideoLinkFromHTML(char *html, char *outputLinkToken) {
  const char *ID_STRING = "videoId";

  // Boolean for when the id key and url value is found
  int foundId = 0;
  int foundUrl = 0;

  // Counter for the quotation marks to figure out when the url starts and end
  int qtMarkCount = 0;

  int i = 0; // Char in outputLinkToken index
  int j = 0;
  char c = html[0];

  // TODO: Maybe use Regex instead
  while (c != '\0') {
    c = html[i++];

    outputLinkToken[j] = c;

    // Searching for "videoId"
    if (!foundId) {
      if (c == ID_STRING[j]) {
        // If character reaches end of string, string is found
        if (j == (int)strlen(ID_STRING) - 1) {
          foundId = 1;
          j = 0;
        } else {
          j++;
        }
      } else {
        // When character doesn't match 'id', reset
        j = 0;
      }
    } else if (!foundUrl) {
      // Count quotation marks
      if (c == '\"') {
        // After the second quotation mark is found, the url is complete
        if (qtMarkCount >= 2) {
          foundUrl = 1;
          outputLinkToken[j] = '\0';
          break;
        }

        qtMarkCount++;
        j = 0; // If there's quotation mark, reset the word starting point
      } else if (c != ':') {
        outputLinkToken[j++] = c;
      }
    }
  }
}

void getVideoTimeStampsFromHTMLFile(FILE *fp) {
  char buf[DESC_LENGTH + 1];

  regex_t regex;
  int regErr;

  /* Compile regular expression */
  regErr = regcomp(&regex, TIMESTAMP_REGEX, 0);

  regmatch_t match[1];

  while (fgets(buf, DESC_LENGTH, fp)) {
    // puts(buf);

    /* Execute regular expression */
    regErr = regexec(&regex, buf, 1, match, 0);

    char matchedString[TIMESTAMP_LENGTH + 1];

    if (!regErr) {

      int numchars = (int)match[0].rm_eo - (int)match[0].rm_so;
      strncpy(matchedString, buf + match[0].rm_so, numchars);
      matchedString[numchars] = '\0';

      printf("(%s)\n", matchedString);
    }
  }
}

// int main(int argc, char *argv[]) {
//   char link[YOUTUBE_VIDEO_URL_SIZE];

//   if (argc > 1) {
//     search_youtube_for_link(argv[1], link);

//     printf("%s\n", link);
//   }
// }