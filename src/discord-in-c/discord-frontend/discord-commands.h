#include "youtube/youtube-func.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOKEN '-'
#define MAX_COMMAND_SIZE 15

/* Commands */
#define CMD_PLAY "p"
#define CMD_SEEK "seek"
#define CMD_SKIP "skip"

void discord_check_message(char *msg, void *state);
void discord_run_command(char *cmd_msg, void *state);

void discord_play_song(char *urlOrSearchTokens);
void discord_skip();
void discord_seek(int position);

/* Helper functions */
#define YOUTUBE_TOKEN "youtube.com/watch?v="

int is_valid_youtube_url(char *inputUrl);

char* get_args(char* message, char* command);