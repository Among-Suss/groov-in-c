#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <sodium.h>
#include <sys/random.h>
#include <opus/opus.h>
#include <ogg/ogg.h>

#include "../../config.h"
#include "sbuf.h"
#include "litesocket/litesocket.h"
#include "discord.h"

typedef struct ffmpeg_process_waiter_t ffmpeg_process_waiter_t;

typedef struct media_player_t media_player_t;

typedef struct youtube_player_t youtube_player_t;

void play_youtube_in_thread(char *youtube_link, char *key_str, char *ssrc_str, char *dest_address, char *dest_port, int socketfd, char *cache_file_unique_name);
media_player_t *start_player(char *key_str, char *ssrc_str,
                            char *dest_address, char *dest_port, int socketfd,
                            char *cache_file_unique_name, voice_gateway_t *vgt);