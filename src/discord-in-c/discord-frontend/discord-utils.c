#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define YOUTUBE_TOKEN "youtube.com/watch?v="

int is_valid_youtube_url(char *inputUrl) {
  return strstr(inputUrl, YOUTUBE_TOKEN) == NULL;
}

int main(int argc, char *argv[]) {
  char str[30];
  while (1) {
    scanf("%s", str);

    printf("%d", is_valid_url(str));
  }
}