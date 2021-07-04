#include "discord-commands.h"

// Functions

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
 * @param cmd_msg Message of the command, not includeing the prefix
 * @param state
 */
void discord_run_command(char *cmd_msg, void *state) {
  char cmd[MAX_COMMAND_SIZE];

  sscanf(cmd_msg, "%s ", cmd);

  printf("Command: %s\n", cmd);

  if (strcmp(cmd, CMD_PLAY) == 0) {
    // Substring to remove the prefixing command and space
    char *urlOrSearchTokens = cmd_msg + (strlen(CMD_PLAY) + 1);
    printf("Substring: %s\n", urlOrSearchTokens);
    discord_play_song(urlOrSearchTokens);
  }
}

/**
 * DISCORD COMMANDS
 *
 */

/**
 * Play command.
 * @param urlOrSearchTokens URL String or search tokens
 */
void discord_play_song(char *urlOrSearchTokens) {
  if (!is_valid_youtube_url(urlOrSearchTokens)) {
    char url[YOUTUBE_VIDEO_URL_SIZE];

    searchYoutubeForLink(urlOrSearchTokens, url);

    strcpy(urlOrSearchTokens, url);
  }

  printf("URL: %s\n", urlOrSearchTokens);
}

int main(int argc, char *argv[]) {
  char buf[200];
  while (1) {
    fgets(buf, 200, stdin);

    discord_check_message(buf, NULL);
    puts("=========");
  }
}