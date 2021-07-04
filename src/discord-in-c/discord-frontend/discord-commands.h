#include "youtube/youtube-func.h"
#include "discord-utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOKEN '-'
#define MAX_COMMAND_SIZE 15
#define CMD_PLAY "p"

void discord_check_message(char *msg, void *state);
void discord_run_command(char *cmd_msg, void *state);
void discord_play_song(char *urlOrSearchTokens);
void discord_play_song(char *urlOrSearchTokens);