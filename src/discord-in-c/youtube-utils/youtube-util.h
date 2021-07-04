#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HTML_BUFFER 4069

/**
 * Parses raw HTML string of a youtube search page for the first search result
 * @param html HTML string to parse
 * @param outputLinkToken Char pointer to the string to store the output
 */
void getVideoLinkFromHTML(char *html, char *outputLinkToken);