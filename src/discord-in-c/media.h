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

//defines from opusrtp.c
#define RTP_HEADER_MIN 12
typedef struct {
  int version;
  int type;
  int pad, ext, cc, mark;
  int seq, time;
  int ssrc;
  int *csrc;
  int header_size;
  int payload_size;
} rtp_header;

typedef struct {
    pid_t pid;
    int *ffmpeg_process_state;
} ffmpeg_process_waiter_t;

typedef struct {
  struct sbuf_t song_queue;
} media_player_t;

typedef struct {
    char *youtube_link;
    char *key_str;
    char *ssrc_str;
    char *dest_address;
    char *dest_port;
    char *cache_file_unique_name;
    media_player_t *media_player_t_ptr;
    int socketfd;
    void *vgt;
} youtube_player_t;

void play_youtube_in_thread(char *youtube_link, char *key_str, char *ssrc_str, char *dest_address, char *dest_port, int socketfd, char *cache_file_unique_name);
media_player_t *start_player(char *key_str, char *ssrc_str,
                            char *dest_address, char *dest_port, int socketfd,
                            char *cache_file_unique_name, void *vgt);