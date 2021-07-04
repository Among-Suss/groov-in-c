#include "discord-commands.h"

/**
 * Figures out whether or not message is a valud command
 */
void discord_check_message(char *msg, void *state) {
  if (msg[0] == TOKEN) {
    char *cmd;

    cmd = msg + 1;

    discord_run_command(cmd, state);
  }
}

/**
 * Figures out what command function to call
 * @param message Message of the command, not includeing the prefix
 * @param state
 */
void discord_run_command(char *message, void *state) {
  char cmd[MAX_COMMAND_SIZE];

  sscanf(message, "%s ", cmd);

  // Extracts the args by getting the substring not including the command
  char *args = message + (strlen(cmd) + 1);

  // printf("Command: %s\n", cmd);

  if (strcmp(cmd, CMD_PLAY) == 0) {
    discord_play_song(args);
  } else if (strcmp(cmd, CMD_SKIP) == 0) {
    discord_skip();
  } else if (strcmp(cmd, CMD_SEEK) == 0) {
    int position = atoi(args);
    discord_seek(position);
  }
}

/* ---------------------------- DISCORD COMMANDS ---------------------------- */

/**
 * Play command.
 * @param urlOrSearchTokens URL String or search tokens
 */
void discord_play_song(char *urlOrSearchTokens) {
  if (!is_valid_youtube_url(urlOrSearchTokens)) {
    char url[YOUTUBE_VIDEO_URL_SIZE];

    search_youtube_for_link(urlOrSearchTokens, url);

    strcpy(urlOrSearchTokens, url);
  }

  printf("URL: %s\n", urlOrSearchTokens);
}

void discord_skip() {}

void discord_seek(int position) { printf("Seeking to %d\n", position); }

/* ---------------------------- HELPER FUNCTIONS ---------------------------- */

/**
 * Checks whether the string is a valid youtube link
 */
int is_valid_youtube_url(char *inputUrl) {
  return strstr(inputUrl, YOUTUBE_TOKEN) != NULL;
}

// Testing

int main(int argc, char *argv[]) {
  char buf[200];
  while (1) {
    fgets(buf, 200, stdin);

    discord_check_message(buf, NULL);
    puts("=========");
  }
}