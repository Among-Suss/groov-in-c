typedef struct discord_t {
  SSL *https_api_ssl;
  SSL *gateway_ssl;
  sem_t gateway_writer_mutex;
  usercallback_f gateway_callback;
  pthread_t gateway_listen_tid;
  int heartbeating;
  int heartbeat_interval_usec;
  pthread_t heartbeat_tid;
  StrMap *data_dictionary;
  StrMap *voice_gateway_map;
} discord_t;


typedef struct voice_gateway_t {
  SSL *voice_ssl;
  usercallback_f voice_callback;
  pthread_t voice_gate_listener_tid;
  sem_t voice_writer_mutex;
  int heartbeating;
  int heartbeat_interval_usec;
  pthread_t heartbeat_tid;
  sem_t ready_state_update;
  sem_t ready_server_update;
  sem_t stream_ready;
  sem_t voice_key_ready;
  StrMap *data_dictionary;
  unsigned char voice_encryption_key[32];
  int voice_udp_sockfd;
} voice_gateway_t;