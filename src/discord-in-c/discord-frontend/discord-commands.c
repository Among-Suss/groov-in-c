#include "../youtube-util/youtube-func.h"
#include "discord-utils.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// For header file
#define TOKEN '-'

#define MAX_COMMAND_SIZE 15

#define CMD_PLAY "p"

// Functions
void discord_check_message(char *msg, void *state) {
  if (msg[0] == TOKEN) {
    char *cmd;

    cmd = msg + 1;

    discord_run_command(cmd, state);
  }
}

void discord_run_command(char *cmd_msg, void *state) {
  char cmd[MAX_COMMAND_SIZE];

  sscanf(cmd_msg, "%s ", cmd);

  if (strcmp(cmd, CMD_PLAY) == 0) {
  }
}

void discord_play_song(char *song) {
  if (is_valid_youtube_url(song)) {

  } else {
  }
}

int main(int argc, char *argv[]) {}