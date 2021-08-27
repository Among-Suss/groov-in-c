#include "discord.h"
#include "discord.structs.h"
#include "litesocket/litesocket.structs.h"

void authenticate_gateway(discord_t *discord, char *discord_intent);
void internal_gateway_callback(SSL *ssl, void *state, char *msg,
                               unsigned long msg_len);
void *threaded_gateway_heartbeat(void *ptr);
void *threaded_voice_gateway_heartbeat(void *ptr);

//first function to be called to inititalize
//discord's gateway
discord_t *init_discord(char *bot_token, char *discord_intent) {
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
  sm_put(sm, DISCORD_PORT_KEY, DISCORD_PORT, 4);

  sm_put(sm, DISCORD_GATEWAY_INTENT, discord_intent, strlen(discord_intent) + 1);

  return discord_data;
}


//These functions deal with cleanup code

//cleanup callback
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

//free function
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









//GATEWAY HANDLER FUNCTIONS ---------------------------------------

//connects to the gateway with the intent
//according to the API
//connects only this particular discord_t obj
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
      gatewayssl, discord_data, internal_gateway_callback);

  authenticate_gateway(discord_data, discord_intent);
}

//handles gateway init heartbeat
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

//handle voice state obj from discord
//when connecting to voice
void update_voice_state(discord_t *discord, char *msg) {
  char *session_id = strcasestr(msg, DISCORD_GATEWAY_VOICE_SESSION_ID);
  session_id += 13;

  char *guild_id = strcasestr(msg, DISCORD_GUILD_ID);
  guild_id += 11;

  char *botid = strcasestr(msg, DISCORD_GATEWAY_VOICE_USERNAME);
  botid = strcasestr(msg, DISCORD_GATEWAY_VOICE_BOT_ID);
  botid += 5;

  char *channel_id = strcasestr(msg, DISCORD_VOICE_STATE_UPDATE_CHANNEL_ID);
  if(channel_id)
    channel_id += 13;

  char *end;

  end = strchr(session_id, '"');
  *end = 0;

  end = strchr(guild_id, '"');
  *end = 0;

  end = strchr(botid, '"');
  *end = 0;

  char *channel_id_end = 0;
  if(channel_id){
    channel_id_end = strchr(channel_id, '"');
    if(channel_id_end){
      *channel_id_end = 0;
    }
  }
  

  voice_gateway_t *vgt;
  int ret = sm_get(discord->voice_gateway_map, guild_id, (char *)&vgt, sizeof(void *));

  fprintf(stdout, "\n\ndebug vc channel id: %s \n\n", channel_id);

  if(ret){
    sm_put(vgt->data_dictionary, DISCORD_GATEWAY_VOICE_SESSION_ID, session_id,
          strlen(session_id) + 1);
    sm_put(vgt->data_dictionary, DISCORD_GATEWAY_VOICE_BOT_ID, botid,
          strlen(botid) + 1);
    if(channel_id_end){
      sm_put(vgt->data_dictionary, DISCORD_VOICE_STATE_UPDATE_CHANNEL_ID, channel_id,
            strlen(channel_id) + 1);
    }
    sem_post(&(vgt->ready_state_update));
  }
}

//handles voice server object from discord
//when connecting to voice
void update_voice_server(discord_t *discord, char *msg) {
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

  sm_put(vgt->data_dictionary, DISCORD_GATEWAY_VOICE_TOKEN, token,
         strlen(token) + 1);
  sm_put(vgt->data_dictionary, DISCORD_GATEWAY_VOICE_ENDPOINT, endpoint,
         strlen(endpoint) + 1);
  sm_put(vgt->data_dictionary, DISCORD_GATEWAY_VOICE_PORT, port,
         strlen(port) + 1);
  sem_post(&(vgt->ready_server_update));
}

//handles gateway ready object in order to
//collect bot id
void gateway_ready(discord_t *discord, char *msg){
  char *bot_id = strcasestr(msg, DISCORD_GATEWAY_USERNAME);
  bot_id = strcasestr(bot_id, DISCORD_GATEWAY_BOT_ID) + 5;

  char *end = strcasestr(bot_id, "\"");
  *end = 0;

  sm_put(discord->data_dictionary, DISCORD_GATEWAY_BOT_ID, bot_id,
    strlen(bot_id) + 1);
}

//function called by litesocket.c bounded socket reader
void internal_gateway_callback(SSL *ssl, void *state, char *msg,
                               unsigned long msg_len) {
  discord_t *discord = state;

  if(!msg){
    pthread_cancel(discord->heartbeat_tid);
    discord->heartbeating = 0;

    disconnect_and_free_ssl(discord->gateway_ssl);
    connect_gateway(discord);
    return;
  }

  msg[msg_len] = 0;

  if (strcasestr(msg, DISCORD_GATEWAY_HEARTBEAT_INFO_OPCODE) &&
      !discord->heartbeating) {
    write(STDOUT_FILENO, msg, msg_len);
    write(STDOUT_FILENO, "\n", 1);
    start_heartbeat_gateway(discord, msg);
    return;
  }

  if (strcasestr(msg, DISCORD_GATEWAY_READY)) {
    write(STDOUT_FILENO, msg, msg_len);
    write(STDOUT_FILENO, "\n", 1);
    gateway_ready(discord, msg);
    return;
  }

  char bot_id[DISCORD_MAX_ID_LEN];
  int ret = sm_get(discord->data_dictionary, DISCORD_GATEWAY_BOT_ID, bot_id, DISCORD_MAX_ID_LEN);

  if (strcasestr(msg, DISCORD_GATEWAY_VOICE_STATE_UPDATE) &&
      ret && strstr(msg, bot_id)) {
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

//bind user's function to the gateway callback
void set_gateway_callback(discord_t *discord, usercallback_f gateway_callback) {
  discord->gateway_callback = gateway_callback;
}

//this function is used internally to authenticate gateway
void authenticate_gateway(discord_t *discord, char *discord_intent) {
  char bot_token[MAX_BOT_TOKEN_LEN];
  sm_get(discord->data_dictionary, BOT_TOKEN_KEY, bot_token, MAX_BOT_TOKEN_LEN);

  int auth_str_len = strlen(DISCORD_GATEWAY_AUTH_STRING);
  char msg[MAX_BOT_TOKEN_LEN + auth_str_len];

  snprintf(msg, MAX_BOT_TOKEN_LEN + auth_str_len, DISCORD_GATEWAY_AUTH_STRING, bot_token, discord_intent);

  sem_wait(&(discord->gateway_writer_mutex));
  send_websocket(discord->gateway_ssl, msg, strlen(msg),
                 WEBSOCKET_OPCODE_MSG);
  sem_post(&(discord->gateway_writer_mutex));
}

//END OF GATEWAY FUNCTIONS ---------------------------------------










//VOICE GATEWAY HANDLER FUNCTIONS -------------------------------

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

  sem_post(&(voice->voice_key_ready));
}

void internal_voice_gateway_callback(SSL *ssl, void *state, char *msg,
                                     unsigned long msg_len) {
  voice_gateway_t *voice = state;

  if(!msg){
    pthread_cancel(voice->heartbeat_tid);
    voice->heartbeating = 0;

    reconnect_voice(voice);

    (voice->reconn_callback)(voice, 0, 0);
    return;
  }

  msg[msg_len] = 0;

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

  //debug
  fprintf(stdout, "authen voice msg: %s\n", msg);

  sem_wait(&(voice->voice_writer_mutex));
  send_websocket(voice->voice_ssl, msg, strlen(msg), WEBSOCKET_OPCODE_MSG);
  sem_post(&(voice->voice_writer_mutex));
}

void authenticate_voice_gateway_reconnect(voice_gateway_t *voice, char *guild_id,
                                char *bot_uid, char *session_id, char *token) {
  char msg[DISCORD_MAX_MSG_LEN];
  snprintf(msg, DISCORD_MAX_MSG_LEN, DISCORD_VOICE_REAUTH_STRING, guild_id,
           session_id, token);

  //debug
  fprintf(stdout, "authen voice msg: %s\n", msg);

  sem_wait(&(voice->voice_writer_mutex));
  send_websocket(voice->voice_ssl, msg, strlen(msg), WEBSOCKET_OPCODE_MSG);
  sem_post(&(voice->voice_writer_mutex));
}

void connect_voice_udp(voice_gateway_t *voice, char *ip, char *port) {
  char buf[DISCORD_MAX_MSG_LEN];

  snprintf(buf, DISCORD_MAX_MSG_LEN, DISCORD_VOICE_ESTABLISH_UDP,
           ip, port,
           DISCORD_VOICE_UDP_ENC_DEFAULT);

  sem_wait(&(voice->voice_writer_mutex));
  send_websocket(voice->voice_ssl, buf, strlen(buf), WEBSOCKET_OPCODE_MSG);
  sem_post(&(voice->voice_writer_mutex));
}

//returns a pointer that MUST BE FREED
char* udp_hole_punch(voice_gateway_t *vgt, int *socketfd_retval){
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
    fprintf(stderr, "Cannot resolve host %s port %s: %s\n",
      ip, port, gai_strerror(ret));
    return NULL;
  }

  fd = socket(addrs->ai_addr->sa_family, SOCK_DGRAM, IPPROTO_UDP);
  //check for fd < 0 Couldn't create socket
  ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));
  //check ret < 0 Couldn't set socket options

  char packet[74];
  memset(packet, 0, 74);
  packet[1] = 0x1;

  packet[3] = 0x46;

  packet[4] = 0x00;
  packet[5] = 0x10;
  packet[6] = 0x04;
  packet[7] = 0x6A;

  ret = sendto(fd, packet, 74, 0,addrs->ai_addr, addrs->ai_addrlen);

  char *buffer = malloc(DISCORD_MAX_MSG_LEN);
  const int sizeofbuffer = DISCORD_MAX_MSG_LEN;

  struct sockaddr_storage src_addr;
  socklen_t src_addr_len=sizeof(src_addr);

  int cnt = 0;
  cnt = recvfrom(fd,buffer,sizeofbuffer,0,(struct sockaddr*)&src_addr,&src_addr_len);
  for(int i = 0; i < cnt; i++){
    printf("%x ", buffer[i]);
  }
  printf("\n");

  *socketfd_retval = fd;
  freeaddrinfo(addrs);

  return buffer;
}

void reconnect_voice(voice_gateway_t *vgt){
  char botuid[DISCORD_MAX_ID_LEN];
  char token[DISCORD_MAX_ID_LEN];
  char sessionid[DISCORD_MAX_ID_LEN];
  char guild_id[DISCORD_MAX_ID_LEN];

  sm_get(vgt->data_dictionary, DISCORD_GATEWAY_VOICE_SESSION_ID, sessionid,
         DISCORD_MAX_ID_LEN);
  sm_get(vgt->data_dictionary, DISCORD_GATEWAY_VOICE_BOT_ID, botuid,
         DISCORD_MAX_ID_LEN);
  sm_get(vgt->data_dictionary, DISCORD_GATEWAY_VOICE_TOKEN, token,
         DISCORD_MAX_ID_LEN);
  sm_get(vgt->data_dictionary, DISCORD_VOICE_GATEWAY_GUILD_ID, guild_id, 
         DISCORD_MAX_ID_LEN);

  //sem_wait(&(vgt->ready_state_update));
  //sem_wait(&(vgt->ready_server_update));
  time_t current_time;
  time(&current_time);
  time_t elapsed = current_time - vgt->last_reconnection_time;
  if(elapsed > 10){
    vgt->reconnection_count = 0;
  }
  vgt->last_reconnection_time = current_time;

  if(vgt->reconnection_count >= 5){
    char channel_id[DISCORD_MAX_ID_LEN];

    sm_get(vgt->data_dictionary, DISCORD_VOICE_STATE_UPDATE_CHANNEL_ID, channel_id,
          DISCORD_MAX_ID_LEN);

    int guild_id_len = strlen(guild_id);
    int channel_id_len = strlen(channel_id);
    int gate_vo_join_len = strlen(DISCORD_GATEWAY_VOICE_JOIN);

    fprintf(stdout, "\n\n\nDEBUG::::::%s\n\n\n", channel_id);

    char *msg = malloc(guild_id_len + channel_id_len + gate_vo_join_len);
    snprintf(msg, guild_id_len + channel_id_len + gate_vo_join_len,
            DISCORD_GATEWAY_VOICE_JOIN, guild_id, channel_id);

    sem_wait(&(vgt->discord->gateway_writer_mutex));
    send_websocket(vgt->discord->gateway_ssl, msg, strlen(msg), WEBSOCKET_OPCODE_MSG);
    sem_post(&(vgt->discord->gateway_writer_mutex));

    fprintf(stdout, "\n\n\n WAITING FOR REPLY FROM READY \n\n\n");

    sem_wait(&(vgt->ready_state_update));

    fprintf(stdout, "\n\n\n reconnecting...... \n\n\n");

    free(msg);
  }

  if(vgt->reconnection_count)
    sleep(1);

  char hostname[MAX_URL_LEN];
  char port[MAX_PORT_LEN];

  unsigned int hostname_len =
      sm_get(vgt->data_dictionary, DISCORD_GATEWAY_VOICE_ENDPOINT, hostname,
             MAX_URL_LEN);
  unsigned int port_len = sm_get(
      vgt->data_dictionary, DISCORD_GATEWAY_VOICE_PORT, port, MAX_PORT_LEN);

  disconnect_and_free_ssl(vgt->voice_ssl);
  SSL *voice_ssl = establish_websocket_connection(
      hostname, hostname_len, port, port_len, DISCORD_VOICE_GT_URI,
      strlen(DISCORD_VOICE_GT_URI));
  vgt->voice_ssl = voice_ssl;

  vgt->voice_gate_listener_tid =
      bind_websocket_listener(voice_ssl, vgt, internal_voice_gateway_callback);

  fprintf(stdout, "\n\n\nDEBUG::::::%s\n\n\n", guild_id);
  if(vgt->reconnection_count){
    authenticate_voice_gateway(vgt, guild_id, botuid, sessionid, token);
  }else{
    authenticate_voice_gateway_reconnect(vgt, guild_id, botuid, sessionid, token);
  }
  vgt->reconnection_count++;
  
  sem_wait(&(vgt->stream_ready));

  int socketfd_udp;

  char *ipdiscovery = udp_hole_punch(vgt, &socketfd_udp);
  vgt->voice_udp_sockfd = socketfd_udp;

  char *ipaddr = ipdiscovery + 8;

  unsigned short portdis;
  char *portdisptr = (char *) &portdis;

  portdisptr[0] = *(ipaddr + 65);
  portdisptr[1] = *(ipaddr + 64);

  fprintf(stdout, "\n\n%d\n%s\n", portdis, ipaddr);

  char portstr[100];
  snprintf(portstr, 100, "%d", portdis);

  connect_voice_udp(vgt, ipaddr, portstr);
  sem_wait(&(vgt->voice_key_ready));

  free(ipdiscovery);
}

voice_gateway_t *connect_voice_gateway(discord_t *discord, char *guild_id, char *channel_id,
                           usercallback_f voice_callback, voice_gateway_reconnection_callback_f reconn_callback) {
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
  sm_put(vgt->data_dictionary, DISCORD_VOICE_GATEWAY_GUILD_ID, guild_id, guild_id_len + 1);

  fprintf(stdout, "\n\n\nDEBUG::::::%s\n\n\n", guild_id);

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

  int socketfd_udp;

  char *ipdiscovery = udp_hole_punch(vgt, &socketfd_udp);
  vgt->voice_udp_sockfd = socketfd_udp;

  char *ipaddr = ipdiscovery + 8;

  unsigned short portdis;
  char *portdisptr = (char *) &portdis;

  portdisptr[0] = *(ipaddr + 65);
  portdisptr[1] = *(ipaddr + 64);

  fprintf(stdout, "\n\n%d\n%s\n", portdis, ipaddr);

  char portstr[100];
  snprintf(portstr, 100, "%d", portdis);

  connect_voice_udp(vgt, ipaddr, portstr);
  sem_wait(&(vgt->voice_key_ready));

  free(ipdiscovery);
  free(msg);

  return vgt;
}

//END OFVOICE GATEWAY HANDLER FUNCTIONS ------------------







//HEARTBEAT FUNCTIONS....

void *threaded_gateway_heartbeat(void *ptr) {
  pthread_detach(pthread_self());
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
  }
}

void *threaded_voice_gateway_heartbeat(void *ptr) {
  pthread_detach(pthread_self());
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
  }
}