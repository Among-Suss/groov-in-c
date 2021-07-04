#include "litesocket/litesocket.h"
#include "strmap.h"
#include <semaphore.h>

#define DISCORD_API_VERSION 9

#define MAX_URL_LEN 2048
#define MAX_PORT_LEN 50
#define HEARTBEAT_STRING_MAX_LEN 1024
#define MAX_BOT_TOKEN_LEN 1024
#define DISCORD_MAX_MSG_LEN 4096
#define DISCORD_MAX_ID_LEN 1024
#define DISCORD_DATA_TABLE_SIZE 100
#define DISCORD_MAX_VOICE_CONNECTIONS 1000

#define DISCORD_HOST "discord.com"
#define DISCORD_PORT "443"

#define DISCORD_GET_GATEWAY_REQUEST                                            \
  "GET /api/v9/gateway HTTP/1.1\r\nHost: discord.com\r\n\r\n"
#define DISCORD_GATEWAY_CONNECT_URI "/?v=9&encoding=json"

#define DISCORD_GATEWAY_HEARTBEAT                                              \
  "{\"op\": 1,\"d\": {},\"s\": null,\"t\": null}"
#define DISCORD_GATEWAY_HEARTBEAT_INFO_OPCODE "\"op\":10"
#define DISCORD_GATEWAY_AUTH_STRING "{\"op\": 2,\"d\": {\"token\": \"%s\",\"intents\": %s,\"properties\": {\"$os\": \"linux\",\"$browser\": \"discord_dot_c\",\"$device\": \"discord_dot_c\"}}}"

#define DISCORD_VOICE_GT_URI "/?v=4"
#define DISCORD_GATEWAY_VOICE_JOIN                                             \
  "{\"op\": 4,\"d\": {\"guild_id\": \"%s\",\"channel_id\": "                   \
  "\"%s\",\"self_mute\": false,\"self_deaf\": false}}"

#define DISCORD_VOICE_AUTH_STRING                                              \
  "{\"op\": 0,\"d\": {\"server_id\": \"%s\",\"user_id\": "                     \
  "\"%s\",\"session_id\": \"%s\",\"token\": \"%s\"}}"

#define DISCORD_VOICE_HEARTBEAT "{\"op\": 3,\"d\": 1501184119560}"
#define DISCORD_VOICE_ESTABLISH_UDP                                            \
  "{\"op\": 1,\"d\": {\"protocol\": \"udp\",\"data\": {\"address\": "          \
  "\"%s\",\"port\": %s,\"mode\": \"%s\"}}}"

#define DISCORD_VOICE_UDP_PORT_DEFAULT "1337"
#define DISCORD_VOICE_UDP_IP_DEFAULT "127.0.0.1"
#define DISCORD_VOICE_UDP_ENC_DEFAULT "xsalsa20_poly1305"

// gateway reply search strings and dict keys
#define DISCORD_GATEWAY_READY "\"t\":\"READY\""
#define DISCORD_GATEWAY_USERNAME "username"
#define DISCORD_GATEWAY_BOT_ID "id"
#define DISCORD_GATEWAY_VOICE_STATE_UPDATE "VOICE_STATE_UPDATE"
#define DISCORD_GATEWAY_VOICE_SERVER_UPDATE "VOICE_SERVER_UPDATE"
#define DISCORD_GATEWAY_HEARTBEAT_INTERVAL "\"heartbeat_interval"
#define DISCORD_GATEWAY_VOICE_SESSION_ID "session_id"
#define DISCORD_GATEWAY_VOICE_USERNAME "username"
#define DISCORD_GATEWAY_VOICE_BOT_ID "id"
#define DISCORD_GATEWAY_VOICE_ENDPOINT "endpoint"
#define DISCORD_GATEWAY_VOICE_TOKEN "token"
#define DISCORD_GATEWAY_VOICE_PORT "voice_port"

// voice gateway search strings and dict keys
#define DISCORD_VOICE_GT_INFO_OPCODE "\"op\":2"
#define DISCORD_VOICE_GT_UDP_HANDSHAKE "\"op\":4"
#define DISCORD_VOICE_PORT "port"
#define DISCORD_VOICE_SSRC "ssrc"
#define DISCORD_VOICE_IP "ip"
#define DISCORD_VOICE_SECRET_KEY "secret_key"

// discord object dict keys
#define BOT_TOKEN_KEY "bot_token"
#define DISCORD_HOSTNAME_KEY "discord_hostname"
#define DISCORD_PORT_KEY "discord_port"
#define DISCORD_HEARTBEAT_MSG_KEY "heartbeat_msg"
#define DISCORD_GUILD_ID "guild_id"

struct discord_t;

typedef void (*usercallback_f)(void *state, char *msg, unsigned long msg_len);

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

typedef struct {
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
} voice_gateway_t;

discord_t *init_discord(char *bot_token);
void free_discord(discord_t *discord);
void connect_gateway(discord_t *discord_data, char *discord_intent);
void set_gateway_callback(discord_t *discord, usercallback_f gateway_callback);
void connect_voice_gateway(discord_t *discord, char *guild_id, char *channel_id,
                           usercallback_f voice_callback);