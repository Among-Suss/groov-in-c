#include "media.h"
#include "discord.structs.h"
#include "litesocket/litesocket.structs.h"
#include "sbuf.structs.h"
#include "media.structs.h"
#include "utils.h"

void authenticate_gateway(discord_t *discord, char *discord_intent);
void internal_gateway_callback(SSL *ssl, void *state, char *msg,
                               unsigned long msg_len);
void *threaded_gateway_heartbeat(void *ptr);
void *threaded_voice_gateway_heartbeat(void *ptr);

// first function to be called to inititalize
// discord's gateway
discord_t *init_discord(char *bot_token, char *discord_intent) {
  discord_t *discord_data = malloc(sizeof(discord_t));
  memset(discord_data, 0, sizeof(discord_t));

  sem_init(&(discord_data->gateway_writer_mutex), 0, 1);
  sem_init(&(discord_data->https_writer_mutex), 0, 1);

  StrMap *sm = sm_new(DISCORD_MAX_VOICE_CONNECTIONS);
  discord_data->voice_gateway_map = sm;

  sm = sm_new(DISCORD_DATA_TABLE_SIZE);
  discord_data->data_dictionary = sm;
  sm_put(sm, BOT_TOKEN_KEY, bot_token, strlen(bot_token) + 1);

  discord_data->user_vc_map = sm_new(EXPECTED_NUM_USERS_MAX);

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
  sm_put(sm, DISCORD_PORT_KEY, DISCORD_PORT, 4);

  sm_put(sm, DISCORD_GATEWAY_INTENT, discord_intent,
         strlen(discord_intent) + 1);

  return discord_data;
}

// These functions deal with cleanup code

void free_voice_gateway(voice_gateway_t *vgt) {
  sem_post(vgt->gateway_thread_exiter);
  pthread_cancel(vgt->heartbeat_tid);

  if (vgt->voice_ssl)
    disconnect_and_free_ssl(vgt->voice_ssl);

  vgt->voice_ssl = 0;

  sm_delete(vgt->data_dictionary);
  free(vgt);
}

// cleanup callback
void enum_callback_delete(const char *key, const char *value, int value_len,
                          const void *obj) {
  voice_gateway_t *vgt = *((void **)value);
  free_voice_gateway(vgt);
}

// free function
void free_discord(discord_t *discord) {
  pthread_cancel(discord->heartbeat_tid);
  pthread_cancel(discord->gateway_listen_tid);

  if (discord->https_api_ssl) {
    disconnect_and_free_ssl(discord->https_api_ssl);
  }
  discord->https_api_ssl = 0;
  if (discord->gateway_ssl) {
    disconnect_and_free_ssl(discord->gateway_ssl);
  }
  discord->gateway_ssl = 0;

  sm_enum(discord->voice_gateway_map, enum_callback_delete, NULL);

  sm_delete(discord->voice_gateway_map);
  sm_delete(discord->data_dictionary);
  sm_delete(discord->user_vc_map);

  free(discord);
}

// GATEWAY HANDLER FUNCTIONS ---------------------------------------

// connects to the gateway with the intent
// according to the API
// connects only this particular discord_t obj
void connect_gateway(discord_t *discord_data) {
  char gateway_host[MAX_URL_LEN];
  char gateway_port[MAX_PORT_LEN];
  char discord_intent[MAX_GATEWAY_INTENT_LEN];

  StrMap *sm = discord_data->data_dictionary;
  sm_get(sm, DISCORD_HOSTNAME_KEY, gateway_host, MAX_URL_LEN);
  sm_get(sm, DISCORD_PORT_KEY, gateway_port, MAX_PORT_LEN);
  sm_get(sm, DISCORD_GATEWAY_INTENT, discord_intent, MAX_GATEWAY_INTENT_LEN);

  SSL *gatewayssl = establish_websocket_connection(
      gateway_host, strlen(gateway_host), gateway_port, strlen(gateway_port),
      DISCORD_GATEWAY_CONNECT_URI, strlen(DISCORD_GATEWAY_CONNECT_URI));

  discord_data->gateway_ssl = gatewayssl;
  discord_data->gateway_listen_tid = bind_websocket_listener(
      gatewayssl, discord_data, internal_gateway_callback,
      &(discord_data->gateway_thread_exiter));

  authenticate_gateway(discord_data, discord_intent);
}

// handles gateway init heartbeat
void start_heartbeat_gateway(discord_t *discord, cJSON *cjs) {
  cJSON *d_cjs = cJSON_GetObjectItem(cjs, "d");
  cJSON *hbp_cjs = cJSON_GetObjectItem(d_cjs, "heartbeat_interval");

  discord->heartbeat_interval_usec = hbp_cjs->valueint * 1000;
  discord->heartbeating = 1;

  pthread_create(&(discord->heartbeat_tid), NULL, threaded_gateway_heartbeat,
                 discord);

  fprintf(stdout, "heartbeat interval: %d\n", hbp_cjs->valueint);
}

// handle voice state obj from discord
// when connecting to voice
void update_voice_state(discord_t *discord, cJSON *cjs) {
  cJSON *d_cjs = cJSON_GetObjectItem(cjs, "d");

  cJSON *userid_cjs = cJSON_GetObjectItem(d_cjs, "user_id");
  char *botid = userid_cjs->valuestring;

  cJSON *sessionid_cjs = cJSON_GetObjectItem(d_cjs, "session_id");
  char *session_id = sessionid_cjs->valuestring;

  cJSON *guildid_cjs = cJSON_GetObjectItem(d_cjs, "guild_id");
  char *guild_id = guildid_cjs->valuestring;

  cJSON *channelid_cjs = cJSON_GetObjectItem(d_cjs, "channel_id");
  char *channel_id = channelid_cjs->valuestring;

  fprintf(stdout,
          "Voice State change: \n  user_id:%s \n  session_id:%s \n  "
          "guild_id:%s \n  channel_id:%s \n",
          botid, session_id, guild_id, channel_id);

  char saved_bot_id[DISCORD_MAX_ID_LEN];
  int ret = sm_get(discord->data_dictionary, DISCORD_GATEWAY_BOT_ID,
                   saved_bot_id, DISCORD_MAX_ID_LEN);
  char *is_bot = 0;
  if (ret) {
    is_bot = strstr(botid, saved_bot_id);
  }

  if (is_bot) {
    voice_gateway_t *vgt = 0;
    int ret = sm_get(discord->voice_gateway_map, guild_id, (char *)&vgt,
                     sizeof(void *));

    if (ret && vgt) {
      sm_put(vgt->data_dictionary, DISCORD_GATEWAY_VOICE_SESSION_ID, session_id,
             strlen(session_id) + 1);
      sm_put(vgt->data_dictionary, DISCORD_GATEWAY_VOICE_USER_ID, botid,
             strlen(botid) + 1);
      if (!cJSON_IsNull(channelid_cjs)) {
        sm_put(vgt->data_dictionary, DISCORD_VOICE_STATE_UPDATE_CHANNEL_ID,
               channel_id, strlen(channel_id) + 1);
        sem_post(&(vgt->ready_state_update));
      } else {
        sm_put(vgt->data_dictionary, DISCORD_VOICE_STATE_UPDATE_CHANNEL_ID, 0,
               sizeof(char));
      }
    }
  } else {
    user_vc_obj uobj = {0};
    strncpy(uobj.guild_id, guild_id, sizeof(uobj.guild_id) - 2);
    if (!cJSON_IsNull(channelid_cjs)) {
      strncpy(uobj.vc_id, channel_id, sizeof(uobj.vc_id) - 2);
    } else {
      uobj.vc_id[0] = 0;
    }
    strncpy(uobj.user_id, botid, sizeof(uobj.user_id) - 2);
    sm_put(discord->user_vc_map, uobj.user_id, (char *)&uobj, sizeof(uobj));
  }
}

// handles voice server object from discord
// when connecting to voice
void update_voice_server(discord_t *discord, cJSON *cjs) {
  cJSON *d_cjs = cJSON_GetObjectItem(cjs, "d");

  cJSON *token_cjs = cJSON_GetObjectItem(d_cjs, "token");
  char *token = token_cjs->valuestring;

  cJSON *guildid_cjs = cJSON_GetObjectItem(d_cjs, "guild_id");
  char *guild_id = guildid_cjs->valuestring;

  cJSON *fullendpoint_cjs = cJSON_GetObjectItem(d_cjs, "endpoint");
  char *endpoint = fullendpoint_cjs->valuestring;

  char *separator = strchr(fullendpoint_cjs->valuestring, ':');
  *separator = 0;
  char *port = separator + 1;

  fprintf(stdout,
          "Voice server change: \n  token:%s \n  guild_id:%s \n  endpoint:%s "
          "\n  port:%s \n",
          token, guild_id, endpoint, port);

  voice_gateway_t *vgt;
  sm_get(discord->voice_gateway_map, guild_id, (char *)&vgt, sizeof(void *));

  sm_put(vgt->data_dictionary, DISCORD_GATEWAY_VOICE_TOKEN, token,
         strlen(token) + 1);
  sm_put(vgt->data_dictionary, DISCORD_GATEWAY_VOICE_ENDPOINT, endpoint,
         strlen(endpoint) + 1);
  sm_put(vgt->data_dictionary, DISCORD_GATEWAY_VOICE_PORT, port,
         strlen(port) + 1);

  char dgvt_notvoice[100];
  char dgve_notvoice[100];
  char dgvp_notvoice[100];

  snprintf(dgvt_notvoice, 100, "%s%s", DISCORD_GATEWAY_VOICE_TOKEN, guild_id);
  snprintf(dgve_notvoice, 100, "%s%s", DISCORD_GATEWAY_VOICE_ENDPOINT,
           guild_id);
  snprintf(dgvp_notvoice, 100, "%s%s", DISCORD_GATEWAY_VOICE_PORT, guild_id);

  sm_put(discord->data_dictionary, dgvt_notvoice, token, strlen(token) + 1);
  sm_put(discord->data_dictionary, dgve_notvoice, endpoint,
         strlen(endpoint) + 1);
  sm_put(discord->data_dictionary, dgvp_notvoice, port, strlen(port) + 1);

  sem_post(&(vgt->ready_server_update));

  *separator = ':';
}

// handles gateway ready object in order to
// collect bot id
void gateway_ready(discord_t *discord, cJSON *cjs) {
  cJSON *d_cjs = cJSON_GetObjectItem(cjs, "d");
  cJSON *user_cjs = cJSON_GetObjectItem(d_cjs, "user");
  cJSON *id_cjs = cJSON_GetObjectItem(user_cjs, "id");

  char *bot_id = id_cjs->valuestring;

  sm_put(discord->data_dictionary, DISCORD_GATEWAY_BOT_ID, bot_id,
         strlen(bot_id) + 1);

  fprintf(stdout, "Gateway Ready received. Bot id: %s\n", bot_id);
}

// function called by litesocket.c bounded socket reader
void internal_gateway_callback(SSL *ssl, void *state, char *msg,
                               unsigned long msg_len) {
  discord_t *discord = state;
  if (!msg) {
    pthread_cancel(discord->heartbeat_tid);
    discord->heartbeating = 0;

    disconnect_and_free_ssl(discord->gateway_ssl);
    discord->gateway_ssl = 0;

    connect_gateway(discord);
    return;
  }
  msg[msg_len] = 0;

  cJSON *cjs = cJSON_ParseWithLength(msg, msg_len);
  cJSON *t_cjs = cJSON_GetObjectItem(cjs, "t");
  cJSON *op_cjs = cJSON_GetObjectItem(cjs, "op");

  if (op_cjs && cJSON_IsNumber(op_cjs) &&
      op_cjs->valueint == DISCORD_GATEWAY_HEARTBEAT_INFO_OPCODE &&
      !discord->heartbeating) {
    fprintf(stdout, "Gateway heartbeat received.\n");
    start_heartbeat_gateway(discord, cjs);
  }

  if (t_cjs && cJSON_IsString(t_cjs) &&
      strcasestr(t_cjs->valuestring, DISCORD_GATEWAY_READY)) {
    fprintf(stdout, "Gateway ready received.\n");
    gateway_ready(discord, cjs);
  }

  if (t_cjs && cJSON_IsString(t_cjs) &&
      strcasestr(t_cjs->valuestring, DISCORD_GATEWAY_VOICE_STATE_UPDATE)) {
    fprintf(stdout, "Updating voice states.\n");
    update_voice_state(discord, cjs);
  }

  if (t_cjs && cJSON_IsString(t_cjs) &&
      strcasestr(t_cjs->valuestring, DISCORD_GATEWAY_VOICE_SERVER_UPDATE)) {
    fprintf(stdout, "Updating voice server.\n");
    update_voice_server(discord, cjs);
  }

  usercallback_f callback = discord->gateway_callback;
  if (callback) {
    callback(discord, msg, msg_len);
  }

  cJSON_Delete(cjs);
}

// bind user's function to the gateway callback
void set_gateway_callback(discord_t *discord, usercallback_f gateway_callback) {
  discord->gateway_callback = gateway_callback;
}

// this function is used internally to authenticate gateway
void authenticate_gateway(discord_t *discord, char *discord_intent) {
  char bot_token[MAX_BOT_TOKEN_LEN];
  sm_get(discord->data_dictionary, BOT_TOKEN_KEY, bot_token, MAX_BOT_TOKEN_LEN);

  int auth_str_len = strlen(DISCORD_GATEWAY_AUTH_STRING);
  char msg[MAX_BOT_TOKEN_LEN + auth_str_len + MAX_GATEWAY_INTENT_LEN];

  snprintf(msg, sizeof(msg), DISCORD_GATEWAY_AUTH_STRING, bot_token,
           discord_intent);

  sem_wait(&(discord->gateway_writer_mutex));
  send_websocket(discord->gateway_ssl, msg, strlen(msg), WEBSOCKET_OPCODE_MSG);
  sem_post(&(discord->gateway_writer_mutex));
}

// END OF GATEWAY FUNCTIONS ---------------------------------------

// VOICE GATEWAY HANDLER FUNCTIONS -------------------------------

void start_heartbeat_voice_gateway(voice_gateway_t *voice, cJSON *cjs) {
  cJSON *d_cjs = cJSON_GetObjectItem(cjs, "d");
  cJSON *hbi_cjs = cJSON_GetObjectItem(d_cjs, "heartbeat_interval");

  voice->heartbeat_interval_usec = hbi_cjs->valuedouble * 1000;
  voice->heartbeating = 1;

  pthread_create(&(voice->heartbeat_tid), NULL,
                 threaded_voice_gateway_heartbeat, voice);

  fprintf(stdout, "Voice heartbeat interval: %f\n", hbi_cjs->valuedouble);
}

void collect_stream_info(voice_gateway_t *voice, cJSON *cjs) {
  cJSON *d_cjs = cJSON_GetObjectItem(cjs, "d");

  cJSON *ssrc_cjs = cJSON_GetObjectItem(d_cjs, "ssrc");
  int ssrc_int = ssrc_cjs->valueint;
  char ssrc[50];
  snprintf(ssrc, sizeof(ssrc), "%d", ssrc_int);

  cJSON *port_cjs = cJSON_GetObjectItem(d_cjs, "port");
  int port_int = port_cjs->valueint;
  char port[50];
  snprintf(port, sizeof(port), "%d", port_int);

  cJSON *ip_cjs = cJSON_GetObjectItem(d_cjs, "ip");
  char *ip = ip_cjs->valuestring;

  sm_put(voice->data_dictionary, DISCORD_VOICE_SSRC, ssrc, strlen(ssrc) + 1);
  sm_put(voice->data_dictionary, DISCORD_VOICE_PORT, port, strlen(port) + 1);
  sm_put(voice->data_dictionary, DISCORD_VOICE_IP, ip, strlen(ip) + 1);

  sem_post(&(voice->stream_ready));
}

void collect_secret_key(voice_gateway_t *voice, cJSON *cjs) {
  cJSON *d_cjs = cJSON_GetObjectItem(cjs, "d");
  cJSON *key_cjs = cJSON_GetObjectItem(d_cjs, "secret_key");

  char keystr[200] = {0};
  char tmp_value[5] = {0};

  cJSON *key_array_value = key_cjs->child;

  snprintf(tmp_value, sizeof(tmp_value), "%hhu", key_array_value->valueint);
  strcat(keystr, tmp_value);
  key_array_value = key_array_value->next;
  while (key_array_value) {
    snprintf(tmp_value, sizeof(tmp_value), ",%hhu", key_array_value->valueint);
    strcat(keystr, tmp_value);
    key_array_value = key_array_value->next;
  }

  fprintf(stdout, "Secret Key for sending voice: %s\n", keystr);

  sm_put(voice->data_dictionary, DISCORD_VOICE_SECRET_KEY, keystr,
         strlen(keystr) + 1);

  sem_post(&(voice->voice_key_ready));
}

void internal_voice_gateway_callback(SSL *ssl, void *state, char *msg,
                                     unsigned long msg_len) {
  voice_gateway_t *voice = state;
  if (!msg) {
    fprintf(stdout, "Disconnected from voice, stopping heartbeat.\n");

    pthread_cancel(voice->heartbeat_tid);
    voice->heartbeating = 0;

    fprintf(stdout, "Attempting to reconnect.\n");
    reconnect_voice(voice);

    (voice->reconn_callback)(voice, 0, 0);
    return;
  }
  msg[msg_len] = 0;

  cJSON *cjs = cJSON_ParseWithLength(msg, msg_len);
  cJSON *op_cjs = cJSON_GetObjectItem(cjs, "op");

  if (op_cjs && op_cjs->valueint == DISCORD_VOICE_GT_HEARTBEAT_OPCODE &&
      !voice->heartbeating) {
    start_heartbeat_voice_gateway(voice, cjs);
  }

  if (op_cjs && op_cjs->valueint == DISCORD_VOICE_GT_INFO_OPCODE) {
    fprintf(stdout, "Collecting voice stream info.\n");
    collect_stream_info(voice, cjs);
  }

  if (op_cjs && op_cjs->valueint == DISCORD_VOICE_GT_UDP_HANDSHAKE) {
    collect_secret_key(voice, cjs);
  }

  usercallback_f callback = voice->voice_callback;
  if (callback) {
    callback(voice, msg, msg_len);
  }

  cJSON_Delete(cjs);
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

void authenticate_voice_gateway_reconnect(voice_gateway_t *voice,
                                          char *guild_id, char *bot_uid,
                                          char *session_id, char *token) {
  char msg[DISCORD_MAX_MSG_LEN];
  snprintf(msg, DISCORD_MAX_MSG_LEN, DISCORD_VOICE_REAUTH_STRING, guild_id,
           session_id, token);

  sem_wait(&(voice->voice_writer_mutex));
  send_websocket(voice->voice_ssl, msg, strlen(msg), WEBSOCKET_OPCODE_MSG);
  sem_post(&(voice->voice_writer_mutex));
}

void connect_voice_udp(voice_gateway_t *voice, char *ip, char *port) {
  char buf[DISCORD_MAX_MSG_LEN];

  snprintf(buf, DISCORD_MAX_MSG_LEN, DISCORD_VOICE_ESTABLISH_UDP, ip, port,
           DISCORD_VOICE_UDP_ENC_DEFAULT);

  sem_wait(&(voice->voice_writer_mutex));
  send_websocket(voice->voice_ssl, buf, strlen(buf), WEBSOCKET_OPCODE_MSG);
  sem_post(&(voice->voice_writer_mutex));
}

// returns a pointer that MUST BE FREED
char *udp_hole_punch(voice_gateway_t *vgt, int *socketfd_retval) {
  char ip[100];
  char port[100];

  sm_get(vgt->data_dictionary, DISCORD_VOICE_IP, ip, 100);
  sm_get(vgt->data_dictionary, DISCORD_VOICE_PORT, port, 100);

  int ret;
  int optval = 0;
  int fd;
  struct addrinfo *addrs;
  struct addrinfo hints;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = 0;
  hints.ai_protocol = IPPROTO_UDP;
  ret = getaddrinfo(ip, port, &hints, &addrs);
  if (ret != 0 || !addrs) {
    fprintf(stderr, "Cannot resolve host %s port %s: %s\n", ip, port,
            gai_strerror(ret));
    return NULL;
  }

  fd = socket(addrs->ai_addr->sa_family, SOCK_DGRAM, IPPROTO_UDP);
  if (fd < 0) {
    fprintf(stderr, "Error openning socket in udp hole punch.\n");
  }
  ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));
  if (ret < 0) {
    fprintf(stderr, "Error setsockopt in udp hole punch.\n");
  }

  char packet[74];
  memset(packet, 0, 74);
  packet[1] = 0x1;

  packet[3] = 0x46;

  packet[4] = 0x00;
  packet[5] = 0x10;
  packet[6] = 0x04;
  packet[7] = 0x6A;

  ret = sendto(fd, packet, 74, 0, addrs->ai_addr, addrs->ai_addrlen);
  if (ret < 0) {
    fprintf(stderr, "Error sending packet in udp holepunch.\n");
  }

  char *buffer = malloc(DISCORD_MAX_MSG_LEN);
  const int sizeofbuffer = DISCORD_MAX_MSG_LEN;

  struct sockaddr_storage src_addr;
  socklen_t src_addr_len = sizeof(src_addr);

  ret = recvfrom(fd, buffer, sizeofbuffer, 0, (struct sockaddr *)&src_addr,
                 &src_addr_len);
  if (ret < 0) {
    fprintf(stderr, "Error receiving UDP for Hole Punching!\n");
  }

  *socketfd_retval = fd;
  freeaddrinfo(addrs);

  return buffer;
}

void cancel_voice_gateway_reconnect(voice_gateway_t *vgt, char *guild_id) {
  // wait for song mutex
  sem_wait(&(vgt->media->insert_song_mutex));

  // skipping and quitting media player
  youtube_page_object_t yobj = {0};
  sbuf_insert_front_value((&(vgt->media->song_queue)), &yobj, sizeof(yobj));
  sem_post(&(vgt->media->quitter));
  sem_post(&(vgt->media->skipper));

  // delete voice gateway object from discord object and request a close
  char *ptr = 0;
  sm_put(vgt->discord->voice_gateway_map, guild_id, (char *)&ptr,
         sizeof(void *));
  send_websocket(vgt->voice_ssl, "request close", strlen("request close"), 8);

  // send message to leave voice channel
  char msg[2000];
  snprintf(msg, 2000, DISCORD_GATEWAY_VOICE_LEAVE, guild_id);
  sem_wait(&(vgt->discord->gateway_writer_mutex));
  send_websocket(vgt->discord->gateway_ssl, msg, strlen(msg),
                 WEBSOCKET_OPCODE_MSG);
  sem_post(&(vgt->discord->gateway_writer_mutex));

  if (vgt->voice_ssl)
    disconnect_and_free_ssl(vgt->voice_ssl);

  vgt->voice_ssl = 0;

  sm_delete(vgt->data_dictionary);
  free(vgt);

  fprintf(stdout, "Successfully left voice channel and cleaned up.\n");

  pthread_exit(NULL);
}

void reconnect_voice(voice_gateway_t *vgt) {
  char botuid[DISCORD_MAX_ID_LEN];
  char token[DISCORD_MAX_ID_LEN];
  char sessionid[DISCORD_MAX_ID_LEN];
  char guild_id[DISCORD_MAX_ID_LEN];

  sm_get(vgt->data_dictionary, DISCORD_GATEWAY_VOICE_SESSION_ID, sessionid,
         DISCORD_MAX_ID_LEN);
  sm_get(vgt->data_dictionary, DISCORD_GATEWAY_VOICE_USER_ID, botuid,
         DISCORD_MAX_ID_LEN);
  sm_get(vgt->data_dictionary, DISCORD_GATEWAY_VOICE_TOKEN, token,
         DISCORD_MAX_ID_LEN);
  sm_get(vgt->data_dictionary, DISCORD_VOICE_GATEWAY_GUILD_ID, guild_id,
         DISCORD_MAX_ID_LEN);

  time_t current_time;
  time(&current_time);
  time_t elapsed = current_time - vgt->last_reconnection_time;
  if (elapsed > 10) {
    vgt->reconnection_count = 0;
  }
  vgt->last_reconnection_time = current_time;

  if (vgt->reconnection_count >= 3) {
    char channel_id[DISCORD_MAX_ID_LEN];

    sm_get(vgt->data_dictionary, DISCORD_VOICE_STATE_UPDATE_CHANNEL_ID,
           channel_id, DISCORD_MAX_ID_LEN);

    int guild_id_len = strlen(guild_id);
    int channel_id_len = strlen(channel_id);
    int gate_vo_join_len = strlen(DISCORD_GATEWAY_VOICE_JOIN);

    char *msg = malloc(guild_id_len + channel_id_len + gate_vo_join_len);
    snprintf(msg, guild_id_len + channel_id_len + gate_vo_join_len,
             DISCORD_GATEWAY_VOICE_JOIN, guild_id, channel_id);

    sem_wait(&(vgt->discord->gateway_writer_mutex));
    send_websocket(vgt->discord->gateway_ssl, msg, strlen(msg),
                   WEBSOCKET_OPCODE_MSG);
    sem_post(&(vgt->discord->gateway_writer_mutex));

    fprintf(stdout, "Waiting for reply [ready_state_update]\n");

    struct timespec tms;
    clock_gettime(CLOCK_REALTIME, &tms);
    tms.tv_sec = tms.tv_sec + 5;
    int smret = sem_timedwait(&(vgt->ready_state_update), &tms);
    if (smret != 0) {
      free(msg);
      cancel_voice_gateway_reconnect(vgt, guild_id);
      return;
    }

    fprintf(stdout, "Reply received... connecting!\n");

    free(msg);
  }

  if (vgt->reconnection_count)
    sleep(1);

  char hostname[MAX_URL_LEN];
  char port[MAX_PORT_LEN];

  unsigned int hostname_len =
      sm_get(vgt->data_dictionary, DISCORD_GATEWAY_VOICE_ENDPOINT, hostname,
             MAX_URL_LEN);
  unsigned int port_len = sm_get(
      vgt->data_dictionary, DISCORD_GATEWAY_VOICE_PORT, port, MAX_PORT_LEN);

  disconnect_and_free_ssl(vgt->voice_ssl);
  vgt->voice_ssl = 0;

  SSL *voice_ssl = establish_websocket_connection(
      hostname, hostname_len, port, port_len, DISCORD_VOICE_GT_URI,
      strlen(DISCORD_VOICE_GT_URI));
  vgt->voice_ssl = voice_ssl;

  vgt->voice_gate_listener_tid =
      bind_websocket_listener(voice_ssl, vgt, internal_voice_gateway_callback,
                              &(vgt->gateway_thread_exiter));

  if (vgt->reconnection_count) {
    authenticate_voice_gateway(vgt, guild_id, botuid, sessionid, token);
  } else {
    authenticate_voice_gateway_reconnect(vgt, guild_id, botuid, sessionid,
                                         token);
  }
  vgt->reconnection_count++;

  sem_wait(&(vgt->stream_ready));

  int socketfd_udp;

  char *ipdiscovery = udp_hole_punch(vgt, &socketfd_udp);
  vgt->voice_udp_sockfd = socketfd_udp;

  char *ipaddr = ipdiscovery + 8;

  unsigned short portdis;
  char *portdisptr = (char *)&portdis;

  portdisptr[0] = *(ipaddr + 65);
  portdisptr[1] = *(ipaddr + 64);

  char portstr[100];
  snprintf(portstr, 100, "%d", portdis);

  connect_voice_udp(vgt, ipaddr, portstr);
  sem_wait(&(vgt->voice_key_ready));

  free(ipdiscovery);
}

void cancel_voice_gateway(voice_gateway_t *vgt, char *guild_id) {
  char *nullptr = 0;

  sm_put(vgt->discord->voice_gateway_map, guild_id, (char *)&nullptr,
         sizeof(void *));
  sm_delete(vgt->data_dictionary);
  free(vgt);
}

voice_gateway_t *
connect_voice_gateway(discord_t *discord, char *guild_id, char *channel_id,
                      usercallback_f voice_callback,
                      voice_gateway_reconnection_callback_f reconn_callback,
                      int wait_server) {
  int guild_id_len = strlen(guild_id);
  int channel_id_len = strlen(channel_id);

  int gate_vo_join_len = strlen(DISCORD_GATEWAY_VOICE_JOIN);

  voice_gateway_t *vgt = malloc(sizeof(voice_gateway_t));
  memset(vgt, 0, sizeof(voice_gateway_t));
  sem_init(&(vgt->voice_writer_mutex), 0, 1);
  sem_init(&(vgt->ready_state_update), 0, 0);
  sem_init(&(vgt->ready_server_update), 0, 0);
  sem_init(&(vgt->stream_ready), 0, 0);
  sem_init(&(vgt->voice_key_ready), 0, 0);
  vgt->data_dictionary = sm_new(DISCORD_DATA_TABLE_SIZE);
  vgt->discord = discord;
  sm_put(vgt->data_dictionary, DISCORD_VOICE_GATEWAY_GUILD_ID, guild_id,
         guild_id_len + 1);

  sm_put(discord->voice_gateway_map, guild_id, (char *)&vgt, sizeof(void *));

  char *msg = malloc(guild_id_len + channel_id_len + gate_vo_join_len);
  snprintf(msg, guild_id_len + channel_id_len + gate_vo_join_len,
           DISCORD_GATEWAY_VOICE_JOIN, guild_id, channel_id);

  fprintf(stdout, "Sending request to join voice channel.\n");
  sem_wait(&(discord->gateway_writer_mutex));
  send_websocket(discord->gateway_ssl, msg, strlen(msg), WEBSOCKET_OPCODE_MSG);
  sem_post(&(discord->gateway_writer_mutex));

  fprintf(stdout, "Waiting for server update.\n");
  struct timespec tms;
  clock_gettime(CLOCK_REALTIME, &tms);
  tms.tv_sec = tms.tv_sec + 5;
  int smret = sem_timedwait(&(vgt->ready_state_update), &tms);
  if (smret != 0) {
    free(msg);
    cancel_voice_gateway(vgt, guild_id);
    return NULL;
  }

  if (wait_server)
    sem_wait(&(vgt->ready_server_update));
  else {
    fprintf(stdout, "Waiting for voice server info. (3 seconds timeout)\n");
    clock_gettime(CLOCK_REALTIME, &tms);
    tms.tv_sec = tms.tv_sec + 3;
    sem_timedwait(&(vgt->ready_server_update), &tms);

    char token[100];
    char port[100];
    char endpoint[100];

    char dgvt_notvoice[100];
    char dgve_notvoice[100];
    char dgvp_notvoice[100];

    snprintf(dgvt_notvoice, 100, "%s%s", DISCORD_GATEWAY_VOICE_TOKEN, guild_id);
    snprintf(dgve_notvoice, 100, "%s%s", DISCORD_GATEWAY_VOICE_ENDPOINT,
             guild_id);
    snprintf(dgvp_notvoice, 100, "%s%s", DISCORD_GATEWAY_VOICE_PORT, guild_id);

    sm_get(discord->data_dictionary, dgvt_notvoice, token, 100);
    sm_get(discord->data_dictionary, dgve_notvoice, endpoint, 100);
    sm_get(discord->data_dictionary, dgvp_notvoice, port, 100);

    sm_put(vgt->data_dictionary, DISCORD_GATEWAY_VOICE_TOKEN, token,
           strlen(token) + 1);
    sm_put(vgt->data_dictionary, DISCORD_GATEWAY_VOICE_ENDPOINT, endpoint,
           strlen(endpoint) + 1);
    sm_put(vgt->data_dictionary, DISCORD_GATEWAY_VOICE_PORT, port,
           strlen(port) + 1);
  }

  // connect to voice and establish callback.....
  vgt->voice_callback = voice_callback;
  vgt->reconn_callback = reconn_callback;

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
      bind_websocket_listener(voice_ssl, vgt, internal_voice_gateway_callback,
                              &(vgt->gateway_thread_exiter));

  char botuid[DISCORD_MAX_ID_LEN];
  char token[DISCORD_MAX_ID_LEN];
  char sessionid[DISCORD_MAX_ID_LEN];

  sm_get(vgt->data_dictionary, DISCORD_GATEWAY_VOICE_SESSION_ID, sessionid,
         DISCORD_MAX_ID_LEN);
  sm_get(vgt->data_dictionary, DISCORD_GATEWAY_VOICE_USER_ID, botuid,
         DISCORD_MAX_ID_LEN);
  sm_get(vgt->data_dictionary, DISCORD_GATEWAY_VOICE_TOKEN, token,
         DISCORD_MAX_ID_LEN);

  authenticate_voice_gateway(vgt, guild_id, botuid, sessionid, token);
  sem_wait(&(vgt->stream_ready));

  int socketfd_udp;

  char *ipdiscovery = udp_hole_punch(vgt, &socketfd_udp);
  vgt->voice_udp_sockfd = socketfd_udp;

  char *ipaddr = ipdiscovery + 8;

  unsigned short portdis;
  char *portdisptr = (char *)&portdis;

  portdisptr[0] = *(ipaddr + 65);
  portdisptr[1] = *(ipaddr + 64);

  char portstr[100];
  snprintf(portstr, 100, "%d", portdis);

  connect_voice_udp(vgt, ipaddr, portstr);
  sem_wait(&(vgt->voice_key_ready));

  free(ipdiscovery);
  free(msg);

  return vgt;
}

// END OFVOICE GATEWAY HANDLER FUNCTIONS ------------------




// TIME SLOT WAITER FUNCTIONS

typedef struct {
#if defined HAVE_MACH_ABSOLUTE_TIME
  /* Apple */
  mach_timebase_info_data_t tbinfo;
  uint64_t target;
#elif defined HAVE_CLOCK_GETTIME && defined CLOCK_REALTIME &&                  \
    defined HAVE_NANOSLEEP
  /* try to use POSIX monotonic clock */
  int initialized;
  clockid_t clock_id;
  struct timespec target;
#else
  /* fall back to the old non-monotonic gettimeofday() */
  int initialized;
  struct timeval target;
#endif
} time_slot_wait_t;

void init_time_slot_wait_1(time_slot_wait_t *pt) {
  memset(pt, 0, sizeof(time_slot_wait_t));
}

/*
 * Wait for the next time slot, which begins delta nanoseconds after the
 * start of the previous time slot, or in the case of the first call at
 * the time of the call.  delta must be in the range 0..999999999.
 */
void wait_for_time_slot_1(unsigned long int delta, time_slot_wait_t *state) {
  
#if defined HAVE_MACH_ABSOLUTE_TIME
  /* Apple */

  if (state->tbinfo.numer == 0) {
    mach_timebase_info(&(state->tbinfo));
    state->target = mach_absolute_time();
  } else {
    state->target +=
        state->tbinfo.numer == state->tbinfo.denom
            ? (uint64_t)delta
            : (uint64_t)delta * state->tbinfo.denom / state->tbinfo.numer;
    mach_wait_until(state->target);
  }
#elif defined HAVE_CLOCK_GETTIME && defined CLOCK_REALTIME &&                  \
    defined HAVE_NANOSLEEP
  /* try to use POSIX monotonic clock */

  if (!state->initialized) {
#if defined CLOCK_MONOTONIC && defined _POSIX_MONOTONIC_CLOCK &&               \
    _POSIX_MONOTONIC_CLOCK >= 0
    if (
#if _POSIX_MONOTONIC_CLOCK == 0
        sysconf(_SC_MONOTONIC_CLOCK) > 0 &&
#endif
        clock_gettime(CLOCK_MONOTONIC, &state->target) == 0) {
      state->clock_id = CLOCK_MONOTONIC;
      state->initialized = 1;
    } else
#endif
        if (clock_gettime(CLOCK_REALTIME, &state->target) == 0) {
      state->clock_id = CLOCK_REALTIME;
      state->initialized = 1;
    }
  } else {
    state->target.tv_nsec += delta;
    
    while (state->target.tv_nsec >= 1000000000) {
      ++state->target.tv_sec;
      state->target.tv_nsec -= 1000000000;
    }
#if defined HAVE_CLOCK_NANOSLEEP && defined _POSIX_CLOCK_SELECTION &&          \
    _POSIX_CLOCK_SELECTION > 0
    clock_nanosleep(state->clock_id, TIMER_ABSTIME, &state->target, NULL);
#else
    {
      /* convert to relative time */
      struct timespec rel;
      if (clock_gettime(clock_id, &rel) == 0) {
        rel.tv_sec = state->target.tv_sec - rel.tv_sec;
        rel.tv_nsec = state->target.tv_nsec - rel.tv_nsec;
        if (rel.tv_nsec < 0) {
          rel.tv_nsec += 1000000000;
          --rel.tv_sec;
        }
        if (rel.tv_sec >= 0 && (rel.tv_sec > 0 || rel.tv_nsec > 0)) {
          nanosleep(&rel, NULL);
        }
      }
    }
#endif
  }
#else
  /* fall back to the old non-monotonic gettimeofday() */
  struct timeval now;
  int nap;

  if (!state->initialized) {
    gettimeofday(&state->target, NULL);
    state->initialized = 1;
  } else {
    delta /= 1000;
    state->target.tv_usec += delta;
    if (state->target.tv_usec >= 1000000) {
      ++state->target.tv_sec;
      state->target.tv_usec -= 1000000;
    }

    gettimeofday(&now, NULL);
    nap = state->target.tv_usec - now.tv_usec;
    if (now.tv_sec != state->target.tv_sec) {
      if (now.tv_sec > state->target.tv_sec)
        nap = 0;
      else if (state->target.tv_sec - now.tv_sec == 1)
        nap += 1000000;
      else
        nap = 1000000;
    }
    if (nap > delta)
      nap = delta;
    if (nap > 0) {
#if defined HAVE_USLEEP
      usleep(nap);
#else
      struct timeval timeout;
      timeout.tv_sec = 0;
      timeout.tv_usec = nap;
      select(0, NULL, NULL, NULL, &timeout);
#endif
    }
  }
#endif
}



// HEARTBEAT FUNCTIONS....

void *threaded_gateway_heartbeat(void *ptr) {
  pthread_detach(pthread_self());
  discord_t *discord = ptr;
  char *heartbeat_str_p = DISCORD_GATEWAY_HEARTBEAT;
  unsigned long msglen = strlen(heartbeat_str_p);

  SSL *ssl = discord->gateway_ssl;
  useconds_t heartbeat_interval = discord->heartbeat_interval_usec;
  unsigned long int hbi_nanosec = ((unsigned long int) heartbeat_interval) * 1000;

  time_slot_wait_t time_state;
  init_time_slot_wait_1(&time_state);
  wait_for_time_slot_1(hbi_nanosec, &time_state);

  fprintf(stdout, "Time slot wait %ld nano seconds %ld microseconds \n", hbi_nanosec, heartbeat_interval);

  sem_t *websock_writer_mutex = &(discord->gateway_writer_mutex);

  while (1) {
    wait_for_time_slot_1(hbi_nanosec, &time_state);
    sem_wait(websock_writer_mutex);
    send_websocket(ssl, heartbeat_str_p, msglen, WEBSOCKET_OPCODE_MSG);
    sem_post(websock_writer_mutex);
  }
}

void *threaded_voice_gateway_heartbeat(void *ptr) {
  pthread_detach(pthread_self());
  voice_gateway_t *voice = ptr;
  char *heartbeat_str_p = DISCORD_VOICE_HEARTBEAT;
  unsigned long msglen = strlen(heartbeat_str_p);

  SSL *ssl = voice->voice_ssl;
  useconds_t heartbeat_interval = voice->heartbeat_interval_usec;
  unsigned long int hbi_nanosec = ((unsigned long int) heartbeat_interval) * 1000;

  time_slot_wait_t time_state;
  init_time_slot_wait_1(&time_state);
  wait_for_time_slot_1(hbi_nanosec, &time_state);

  fprintf(stdout, "Time slot wait %ld nano seconds %ld microseconds \n", hbi_nanosec, heartbeat_interval);

  sem_t *websock_writer_mutex = &(voice->voice_writer_mutex);

  while (1) {
    wait_for_time_slot_1(hbi_nanosec, &time_state);
    sem_wait(websock_writer_mutex);
    send_websocket(ssl, heartbeat_str_p, msglen, WEBSOCKET_OPCODE_MSG);
    sem_post(websock_writer_mutex);
  }
}