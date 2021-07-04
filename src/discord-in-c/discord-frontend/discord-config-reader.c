/**
 * I don't think we'll actually use this
 */

#include "../strmap.h"
#include <stdio.h>
#include <stdlib.h>

#define CONFIG_FILE "bot.cfg"
#define LINE_SIZE 300 // The buffer

#define MAX_CONFIGS 10

int get_configs(StrMap *configs) {
  FILE *fp;
  char buf[LINE_SIZE];

  int i = 0;

  if ((fp = fopen(CONFIG_FILE, "r")) != NULL) {
    while (fgets(buf, LINE_SIZE, fp) != 0) {
      char option[LINE_SIZE];
      char value[LINE_SIZE];

      sscanf(buf, "%s %s", option, value);

      sm_put(configs, option, value, strlen(value));

      i++;
    }
  }

  fclose(fp);

  return i;
}

int main(void) {}