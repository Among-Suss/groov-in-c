#include "discord.h"

void authenticate_gateway(discord_t *discord);
void internal_gateway_callback(SSL *ssl, void *state, char *msg,
                               unsigned long msg_len);

discord_t *init_discord(char *bot_token) {
  discord_t *discord_data = malloc(sizeof(discord_t));
  memset(discord_data, 0, sizeof(discord_t));

  sem_init(&(discord_data->gateway_writer_mutex), 0, 1);

  StrMap *sm;

  sm = sm_new(DISCORD_MAX_VOICE_CONNECTIONS);
  discord_data->voice_gateway_map = sm;

  sm = sm_new(DISCORD_DATA_TABLE_SIZE);
  discord_data->data_dictionary = sm;
  sm_put(sm, BOT_TOKEN_KEY, bot_token, strlen(bot_token) + 1);

  SSL *api_ssl = establish_ssl_connection(DISCORD_HOST, strlen(DISCORD_HOST),
                                          DISCORD_PORT, strlen(DISCORD_PORT));
  discord_data->https_api_ssl = api_ssl;

  send_raw(api_ssl, DISCORD_GET_GATEWAY_REQUEST,
           strlen(DISCORD_GET_GATEWAY_REQUEST));

  char headerbuf[HTTP_MAX_RESPONSE];
  unsigned long readlen;
  read_http_header(api_ssl, headerbuf, HTTP_MAX_RESPONSE, &readlen);

  unsigned long contentlen = get_http_content_length(headerbuf, readlen);
  read_raw(api_ssl, headerbuf, contentlen);
  char *start = strcasestr(headerbuf, "wss://");
  start += 6;
  char *end = strchr(start, '"');
  *end = 0;

  sm_put(sm, DISCORD_HOSTNAME_KEY, start, strlen(start) + 1);
  sm_put(sm, DISCORD_PORT_KEY, "443", 4);

  return discord_data;
}

void enum_callback_delete(const char *key, const char *value, int value_len,
                          const void *obj) {
  voice_gateway_t *vgt = *((void **)value);
  pthread_cancel(vgt->voice_gate_listener_tid);
  pthread_cancel(vgt->heartbeat_tid);
  if (vgt->voice_ssl)
    disconnect_and_free_ssl(vgt->voice_ssl);
  sm_delete(vgt->data_dictionary);
  free(vgt);
}

void free_discord(discord_t *discord) {
  pthread_cancel(discord->heartbeat_tid);
  pthread_cancel(discord->gateway_listen_tid);

  if (discord->https_api_ssl) {
    disconnect_and_free_ssl(discord->https_api_ssl);
  }
  if (discord->gateway_ssl) {
    disconnect_and_free_ssl(discord->gateway_ssl);
  }

  sm_enum(discord->voice_gateway_map, enum_callback_delete, NULL);

  sm_delete(discord->voice_gateway_map);
  sm_delete(discord->data_dictionary);

  free(discord);
}

void connect_gateway(discord_t *discord_data) {
  char gateway_host[MAX_URL_LEN];
  char gateway_port[MAX_PORT_LEN];

  StrMap *sm = discord_data->data_dictionary;
  sm_get(sm, DISCORD_HOSTNAME_KEY, gateway_host, MAX_URL_LEN);
  sm_get(sm, DISCORD_PORT_KEY, gateway_port, MAX_PORT_LEN);

  SSL *gatewayssl = establish_websocket_connection(
      gateway_host, strlen(gateway_host), gateway_port, strlen(gateway_port),
      DISCORD_GATEWAY_CONNECT_URI, strlen(DISCORD_GATEWAY_CONNECT_URI));

  discord_data->gateway_ssl = gatewayssl;
  discord_data->gateway_listen_tid = bind_websocket_listener(
      gatewayssl, discord_data, internal_gateway_callback);

  authenticate_gateway(discord_data);
}

void *threaded_gateway_heartbeat(void *ptr) {
  discord_t *discord = ptr;
  char *heartbeat_str_p = DISCORD_GATEWAY_HEARTBEAT;
  unsigned long msglen = strlen(heartbeat_str_p);

  SSL *ssl = discord->gateway_ssl;
  useconds_t heartbeat_interval = discord->heartbeat_interval_usec;

  sem_t *websock_writer_mutex = &(discord->gateway_writer_mutex);

  while (1) {
    usleep(heartbeat_interval);
    sem_wait(websock_writer_mutex);
    send_websocket(ssl, heartbeat_str_p, msglen, WEBSOCKET_OPCODE_MSG);
    sem_post(websock_writer_mutex);
    write(STDOUT_FILENO, "\n------HEARTBEAT SENT------\n",
          strlen("\n------HEARTBEAT SENT------\n"));
  }
}

void start_heartbeat_gateway(discord_t *discord, char *msg) {
  char *heartbeatp = strcasestr(msg, DISCORD_GATEWAY_HEARTBEAT_INTERVAL);
  heartbeatp += 21;
  char *hbp_end = strchr(heartbeatp, ',');
  *hbp_end = 0;

  discord->heartbeat_interval_usec = atoi(heartbeatp) * 1000;
  discord->heartbeating = 1;

  pthread_create(&(discord->heartbeat_tid), NULL, threaded_gateway_heartbeat,
                 discord);
}

void update_voice_state(discord_t *discord, char *msg) {
  char *session_id = strcasestr(msg, DISCORD_GATEWAY_VOICE_SESSION_ID);
  session_id += 13;

  char *guild_id = strcasestr(msg, DISCORD_GUILD_ID);
  guild_id += 11;

  char *botid = strcasestr(msg, DISCORD_GATEWAY_VOICE_USERNAME);
  botid = strcasestr(msg, DISCORD_GATEWAY_VOICE_BOT_ID);
  botid += 5;

  char *end;

  end = strchr(session_id, '"');
  *end = 0;

  end = strchr(guild_id, '"');
  *end = 0;

  end = strchr(botid, '"');
  *end = 0;

  voice_gateway_t *vgt;
  sm_get(discord->voice_gateway_map, guild_id, (char *)&vgt, sizeof(void *));
  printf("......>>>>>>>%d\n", vgt->heartbeat_interval_usec);

  sm_put(vgt->data_dictionary, DISCORD_GATEWAY_VOICE_SESSION_ID, session_id,
         strlen(session_id) + 1);
  sm_put(vgt->data_dictionary, DISCORD_GATEWAY_VOICE_BOT_ID, botid,
         strlen(botid) + 1);
  sem_post(&(vgt->ready_state_update));
}

void update_voice_server(discord_t *discord, char *msg) {
  printf("\n\nwowowowreeeeeee\n\n");
  char *token = strcasestr(msg, DISCORD_GATEWAY_VOICE_TOKEN);
  token += 8;

  char *endpoint = strcasestr(msg, DISCORD_GATEWAY_VOICE_ENDPOINT);
  endpoint += 11;

  char *guild_id = strcasestr(msg, DISCORD_GUILD_ID);
  guild_id += 11;

  char *end;

  end = strchr(token, '"');
  *end = 0;

  end = strchr(endpoint, '"');
  *end = 0;

  end = strchr(guild_id, '"');
  *end = 0;

  char *port = strchr(endpoint, ':');
  *port = 0;
  port++;

  voice_gateway_t *vgt;
  sm_get(discord->voice_gateway_map, guild_id, (char *)&vgt, sizeof(void *));
  printf("----,,,,,,,,%d\n", vgt->heartbeat_interval_usec);

  sm_put(vgt->data_dictionary, DISCORD_GATEWAY_VOICE_TOKEN, token,
         strlen(token) + 1);
  sm_put(vgt->data_dictionary, DISCORD_GATEWAY_VOICE_ENDPOINT, endpoint,
         strlen(endpoint) + 1);
  sm_put(vgt->data_dictionary, DISCORD_GATEWAY_VOICE_PORT, port,
         strlen(port) + 1);
  sem_post(&(vgt->ready_server_update));
}

void internal_gateway_callback(SSL *ssl, void *state, char *msg,
                               unsigned long msg_len) {
  discord_t *discord = state;
  msg[msg_len] = 0;
  if (strcasestr(msg, DISCORD_GATEWAY_HEARTBEAT_INFO_OPCODE) &&
      !discord->heartbeating) {
    start_heartbeat_gateway(discord, msg);
    return;
  }

  if (strcasestr(msg, DISCORD_GATEWAY_VOICE_STATE_UPDATE) &&
      strstr(msg, "835551242246815754")) {
    write(STDOUT_FILENO, msg, msg_len);
    write(STDOUT_FILENO, "\n", 1);
    update_voice_state(discord, msg);
    return;
  }

  if (strcasestr(msg, DISCORD_GATEWAY_VOICE_SERVER_UPDATE)) {
    write(STDOUT_FILENO, msg, msg_len);
    write(STDOUT_FILENO, "\n", 1);
    update_voice_server(discord, msg);
    return;
  }

  usercallback_f callback = discord->gateway_callback;
  if (callback) {
    callback(discord, msg, msg_len);
  }
}

void set_gateway_callback(discord_t *discord, usercallback_f gateway_callback) {
  discord->gateway_callback = gateway_callback;
}

void authenticate_gateway(discord_t *discord) {
  char bot_token[MAX_BOT_TOKEN_LEN];
  sm_get(discord->data_dictionary, BOT_TOKEN_KEY, bot_token, MAX_BOT_TOKEN_LEN);

  int auth_str_len = strlen(DISCORD_GATEWAY_AUTH_STRING);
  char msg[MAX_BOT_TOKEN_LEN + auth_str_len];

  snprintf(msg, MAX_BOT_TOKEN_LEN + auth_str_len, DISCORD_GATEWAY_AUTH_STRING, bot_token);

  sem_wait(&(discord->gateway_writer_mutex));
  send_websocket(discord->gateway_ssl, msg, strlen(msg),
                 WEBSOCKET_OPCODE_MSG);
  sem_post(&(discord->gateway_writer_mutex));
}

void *threaded_voice_gateway_heartbeat(void *ptr) {
  voice_gateway_t *voice = ptr;
  char *heartbeat_str_p = DISCORD_VOICE_HEARTBEAT;
  unsigned long msglen = strlen(heartbeat_str_p);

  SSL *ssl = voice->voice_ssl;
  useconds_t heartbeat_interval = voice->heartbeat_interval_usec;

  sem_t *websock_writer_mutex = &(voice->voice_writer_mutex);

  while (1) {
    usleep(heartbeat_interval);
    sem_wait(websock_writer_mutex);
    send_websocket(ssl, heartbeat_str_p, msglen, WEBSOCKET_OPCODE_MSG);
    sem_post(websock_writer_mutex);
    write(STDOUT_FILENO, "\n------HEARTBEAT SENT------\n",
          strlen("\n------HEARTBEAT SENT------\n"));
  }
}

void start_heartbeat_voice_gateway(voice_gateway_t *voice, char *msg) {
  char *heartbeatp = strcasestr(msg, DISCORD_GATEWAY_HEARTBEAT_INTERVAL);
  heartbeatp += 21;

  char *hbp_end = strchr(heartbeatp, '}');
  *hbp_end = 0;

  voice->heartbeat_interval_usec = atoi(heartbeatp) * 1000;
  voice->heartbeating = 1;

  pthread_create(&(voice->heartbeat_tid), NULL,
                 threaded_voice_gateway_heartbeat, voice);
}

void collect_stream_info(voice_gateway_t *voice, char *msg) {
  char *ssrc = strcasestr(msg, DISCORD_VOICE_SSRC) + 4;
  ssrc = strcasestr(msg, DISCORD_VOICE_SSRC) + 6;

  char *port = strcasestr(msg, DISCORD_VOICE_PORT) + 6;

  char *ip = strcasestr(msg, DISCORD_VOICE_IP) + 5;

  char *end;
  end = strchr(ssrc, ',');
  *end = 0;

  end = strchr(port, ',');
  *end = 0;

  end = strchr(ip, '"');
  *end = 0;

  sm_put(voice->data_dictionary, DISCORD_VOICE_SSRC, ssrc, strlen(ssrc) + 1);
  sm_put(voice->data_dictionary, DISCORD_VOICE_PORT, port, strlen(port) + 1);
  sm_put(voice->data_dictionary, DISCORD_VOICE_IP, ip, strlen(ip) + 1);

  sem_post(&(voice->stream_ready));
}

void collect_secret_key(voice_gateway_t *voice, char *msg) {
  unsigned char *key = voice->voice_encryption_key;
  char *keystr = strcasestr(msg, DISCORD_VOICE_SECRET_KEY) + 13;
  char *end = strchr(keystr, ']');
  *end = 0;

  sm_put(voice->data_dictionary, DISCORD_VOICE_SECRET_KEY, keystr,
         strlen(keystr) + 1);

  int i = 0;
  while ((end = strchr(keystr, ','))) {
    *end = 0;
    key[i] = atoi(keystr);
    keystr = end + 1;
    i++;
  }

  key[31] = atoi(keystr);
}

void internal_voice_gateway_callback(SSL *ssl, void *state, char *msg,
                                     unsigned long msg_len) {
  voice_gateway_t *voice = state;
  if (strcasestr(msg, DISCORD_GATEWAY_HEARTBEAT_INTERVAL) &&
      !voice->heartbeating) {
    write(STDOUT_FILENO, msg, msg_len);
    write(STDOUT_FILENO, "\n", 1);
    start_heartbeat_voice_gateway(voice, msg);
    return;
  }

  if (strcasestr(msg, DISCORD_VOICE_GT_INFO_OPCODE)) {
    write(STDOUT_FILENO, msg, msg_len);
    write(STDOUT_FILENO, "\n", 1);
    collect_stream_info(voice, msg);
    return;
  }

  if (strcasestr(msg, DISCORD_VOICE_GT_UDP_HANDSHAKE)) {
    collect_secret_key(voice, msg);
    return;
  }

  usercallback_f callback = voice->voice_callback;
  if (callback) {
    callback(voice, msg, msg_len);
  }
}

void authenticate_voice_gateway(voice_gateway_t *voice, char *guild_id,
                                char *bot_uid, char *session_id, char *token) {
  char msg[DISCORD_MAX_MSG_LEN];
  snprintf(msg, DISCORD_MAX_MSG_LEN, DISCORD_VOICE_AUTH_STRING, guild_id,
           bot_uid, session_id, token);

  sem_wait(&(voice->voice_writer_mutex));
  send_websocket(voice->voice_ssl, msg, strlen(msg), WEBSOCKET_OPCODE_MSG);
  sem_post(&(voice->voice_writer_mutex));
}

void connect_voice_udp(voice_gateway_t *voice) {
  char buf[DISCORD_MAX_MSG_LEN];

  snprintf(buf, DISCORD_MAX_MSG_LEN, DISCORD_VOICE_ESTABLISH_UDP,
           DISCORD_VOICE_UDP_IP_DEFAULT, DISCORD_VOICE_UDP_PORT_DEFAULT,
           DISCORD_VOICE_UDP_ENC_DEFAULT);

  sem_wait(&(voice->voice_writer_mutex));
  send_websocket(voice->voice_ssl, buf, strlen(buf), WEBSOCKET_OPCODE_MSG);
  sem_post(&(voice->voice_writer_mutex));
}

void connect_voice_gateway(discord_t *discord, char *guild_id, char *channel_id,
                           usercallback_f voice_callback) {
  int guild_id_len = strlen(guild_id);
  int channel_id_len = strlen(channel_id);

  int gate_vo_join_len = strlen(DISCORD_GATEWAY_VOICE_JOIN);

  voice_gateway_t *vgt = malloc(sizeof(voice_gateway_t));
  memset(vgt, 0, sizeof(voice_gateway_t));
  sem_init(&(vgt->voice_writer_mutex), 0, 1);
  sem_init(&(vgt->ready_state_update), 0, 0);
  sem_init(&(vgt->ready_server_update), 0, 0);
  sem_init(&(vgt->stream_ready), 0, 0);
  vgt->data_dictionary = sm_new(DISCORD_DATA_TABLE_SIZE);
  printf("......%d\n", vgt);
  sm_put(discord->voice_gateway_map, guild_id, (char *)&vgt, sizeof(void *));

  char *msg = malloc(guild_id_len + channel_id_len + gate_vo_join_len);
  snprintf(msg, guild_id_len + channel_id_len + gate_vo_join_len,
           DISCORD_GATEWAY_VOICE_JOIN, guild_id, channel_id);

  sem_wait(&(discord->gateway_writer_mutex));
  send_websocket(discord->gateway_ssl, msg, strlen(msg), WEBSOCKET_OPCODE_MSG);
  sem_post(&(discord->gateway_writer_mutex));

  sem_wait(&(vgt->ready_state_update));
  sem_wait(&(vgt->ready_server_update));

  // connect to voice and establish callback.....
  vgt->voice_callback = voice_callback;

  char hostname[MAX_URL_LEN];
  char port[MAX_PORT_LEN];

  unsigned int hostname_len =
      sm_get(vgt->data_dictionary, DISCORD_GATEWAY_VOICE_ENDPOINT, hostname,
             MAX_URL_LEN);
  unsigned int port_len = sm_get(
      vgt->data_dictionary, DISCORD_GATEWAY_VOICE_PORT, port, MAX_PORT_LEN);

  SSL *voice_ssl = establish_websocket_connection(
      hostname, hostname_len, port, port_len, DISCORD_VOICE_GT_URI,
      strlen(DISCORD_VOICE_GT_URI));
  vgt->voice_ssl = voice_ssl;

  vgt->voice_gate_listener_tid =
      bind_websocket_listener(voice_ssl, vgt, internal_voice_gateway_callback);

  char botuid[DISCORD_MAX_ID_LEN];
  char token[DISCORD_MAX_ID_LEN];
  char sessionid[DISCORD_MAX_ID_LEN];

  sm_get(vgt->data_dictionary, DISCORD_GATEWAY_VOICE_SESSION_ID, sessionid,
         DISCORD_MAX_ID_LEN);
  sm_get(vgt->data_dictionary, DISCORD_GATEWAY_VOICE_BOT_ID, botuid,
         DISCORD_MAX_ID_LEN);
  sm_get(vgt->data_dictionary, DISCORD_GATEWAY_VOICE_TOKEN, token,
         DISCORD_MAX_ID_LEN);

  authenticate_voice_gateway(vgt, guild_id, botuid, sessionid, token);

  sem_wait(&(vgt->stream_ready));

  connect_voice_udp(vgt);

  free(msg);
}