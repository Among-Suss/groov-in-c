#define _GNU_SOURCE

#include <netdb.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <unistd.h>
#include <semaphore.h>

#define WEBSOCKET_VERSION 13
#define HTTP_MAX_RESPONSE 8192
#define WEBSOCKET_PAYLOAD_BUFFER_LENGTH 8192

#define WEBSOCKET_OPCODE_MSG 0x1
#define WEBSOCKET_OPCODE_CLOSE 0x8

#define MACRO_MIN(x, y) (x < y ? x : y)

typedef void (*callback_func_t)(SSL *ssl, void *state, char *msg,
                                unsigned long msg_len);

typedef struct callback_t callback_t;

void init_openssl_2();
void disconnect_and_free_ssl(SSL *ssl);
int read_http_header(SSL *ssl, char *headerbuf, unsigned long buffer_len,
                     unsigned long *read_len);
unsigned long get_http_content_length(char *header, unsigned long len);
int read_websocket_header(SSL *ssl, unsigned long *msg_len, int *msg_fin);
char *simple_receive_websocket(SSL *ssl, unsigned long *read_len, int *msg_fin);
int send_websocket(SSL *ssl, char *msg, unsigned long msglen, int opcode);
int send_raw(SSL *ssl, char *data, unsigned long len);
int read_raw(SSL *ssl, char *buffer, unsigned long len);
void random_base64(char *buffer, int len);
SSL *ssl_reconnect(SSL *ssl, char *hostname, int hostname_len, char *port,
                   int port_len);
SSL *establish_ssl_connection(char *hostname, int hostname_len, char *port,
                              int port_len);
SSL *establish_websocket_connection(char *hostname, int hostname_len,
                                    char *port, int port_len, char *request_uri,
                                    int request_uri_len);
void *threaded_receive_websock(void *ptr);
pthread_t bind_websocket_listener(SSL *ssl, void *state,
                                  callback_func_t callback, sem_t **exiter);
