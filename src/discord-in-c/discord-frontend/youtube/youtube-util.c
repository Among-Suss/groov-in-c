#include "youtube-util.h"

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
