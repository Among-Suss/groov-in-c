#define _GNU_SOURCE

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
#include "cJSON.h"

#define YOUTUBE_WEBPAGE_URL_SIZE 2048
#define MAX_URL_LEN_MEDIA 16384

#define PLATFORM_YOUTUBE_ID 0
#define PLATFORM_SOUNDCLOUD_ID 1

#define FORMAT_M4A "bestaudio[ext=m4a]"
#define FORMAT_MP3 "bestaudio[ext=mp3]"

typedef struct ffmpeg_process_waiter_t ffmpeg_process_waiter_t;

typedef struct media_player_t media_player_t;

typedef struct youtube_player_t youtube_player_t;

typedef struct youtube_page_object_t youtube_page_object_t;

media_player_t *start_player(char *key_str, char *ssrc_str, char *dest_address,
                             char *dest_port, int socketfd,
                             char *cache_file_unique_name,
                             voice_gateway_t *vgt);

media_player_t *modify_player(media_player_t *media, char *key_str,
                              char *ssrc_str, char *dest_address,
                              char *dest_port, int socketfd,
                              char *cache_file_unique_name,
                              voice_gateway_t *vgt);

int insert_queue_ydl_query(media_player_t *media, char *ydl_query,
                           char *return_title, int return_title_len, int index);

void insert_queue_ytb_partial(media_player_t *media, char *id, char *title,
                              char *duration, int length);

int insert_queue_soundcloud(media_player_t *media, char *snd_cld_query,
                            char *return_title, int return_title_len,
                            int index);

void complete_youtube_object_fields(youtube_page_object_t *ytobjptr);

void seek_media_player(media_player_t *media, int time_in_seconds);

void shuffle_media_player(media_player_t *media);

void clear_media_player(media_player_t *media);