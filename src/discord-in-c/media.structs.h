typedef struct ffmpeg_process_waiter_t{
    pid_t pid;
    int *ffmpeg_process_state;
} ffmpeg_process_waiter_t;

typedef struct youtube_page_object_t{
  char query[1024];
  char link[YOUTUBE_WEBPAGE_URL_SIZE];
  char title[1024];
  char description[2048];
  int length_in_seconds;
} youtube_page_object_t;

typedef struct media_player_t{
  sbuf_t song_queue;
  sem_t skipper;
  volatile int playing;

  volatile int udp_fd;
  struct sockaddr *addr;
  socklen_t addrlen;
  unsigned char key[32];

  sem_t destination_info_mutex;

  youtube_player_t *ytp;

  pid_t player_thread_id;

  sem_t quitter;

  int initialized;
} media_player_t;

typedef struct youtube_player_t{
    char *youtube_link;
    char *key_str;
    char *ssrc_str;
    char *dest_address;
    char *dest_port;
    char *cache_file_unique_name;
    media_player_t *media_player_t_ptr;
    int socketfd;
    voice_gateway_t *vgt;
} youtube_player_t;