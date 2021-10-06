#include "litesocket.h"
#include "litesocket.structs.h"
#include "../utils.h"

/* Helper functions for websocket connections */

/*
 *
 *  Inits openssl
 */
void init_openssl() {
  SSL_library_init();
  SSL_load_error_strings();
}

/*
 *
 *  Free ssl object
 */
void disconnect_and_free_ssl(SSL *ssl) {
  if (ssl) {
    int sockfd = SSL_get_rfd(ssl);
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);

    SSL_shutdown(ssl);
    SSL_shutdown(ssl);

    sleep(1);

    SSL_free(ssl);
  }
}

/*
 *
 *  Reads HTTP header
 */
int read_http_header(SSL *ssl, char *headerbuf, unsigned long buffer_len,
                     unsigned long *read_len) {
  char *hbp = headerbuf;

  char four[4];
  four[0] = 0;
  four[1] = 0;
  four[2] = 0;
  four[3] = 0;

  char buf[10];
  int len;
  unsigned long total_len = 0;

  do {
    if ((len = SSL_read(ssl, buf, 1)) < 0) {
      return -1;
    }

    *hbp = buf[0];
    hbp++;
    total_len++;

    four[0] = four[1];
    four[1] = four[2];
    four[2] = four[3];
    four[3] = buf[0];

  } while (len > 0 &&
           !(four[0] == '\r' && four[1] == '\n' && four[2] == '\r' &&
             four[3] == '\n') &&
           total_len < buffer_len);

  *read_len = total_len;
  return 0;
}

/*
 *
 *  Reads websocket header and writes msg_len and msg_fin:
 *  msg_len is the length of payload
 *  msg_fin is the fin bit in websocket
 */
int read_websocket_header(SSL *ssl, unsigned long *msg_len, int *msg_fin) {
  unsigned char frameHeader[16];
  memset(frameHeader, 0, sizeof(frameHeader));
  unsigned long plen = 0;

  if (SSL_read(ssl, frameHeader, 2) < 0) {
    return -1;
  }

  *msg_fin = frameHeader[0] >> 7;
  plen = frameHeader[1] & 127;

  if (plen < 126) {
    *msg_len = plen;
  } else if (plen == 126) {
    SSL_read(ssl, frameHeader + 2, 2);

    ((unsigned char *)(&plen))[1] = frameHeader[2];
    ((unsigned char *)(&plen))[0] = frameHeader[3];

    *msg_len = plen;
  } else if (plen == 127) {
    SSL_read(ssl, frameHeader + 2, 8);

    ((unsigned char *)(&plen))[7] = frameHeader[2];
    ((unsigned char *)(&plen))[6] = frameHeader[3];
    ((unsigned char *)(&plen))[5] = frameHeader[4];
    ((unsigned char *)(&plen))[4] = frameHeader[5];
    ((unsigned char *)(&plen))[3] = frameHeader[6];
    ((unsigned char *)(&plen))[2] = frameHeader[7];
    ((unsigned char *)(&plen))[1] = frameHeader[8];
    ((unsigned char *)(&plen))[0] = frameHeader[9];

    *msg_len = plen;
  } else {
    *msg_len = -1;
  }

  return 0;
}

unsigned long get_http_content_length(char *header, unsigned long len) {
  char *start = strcasestr(header, "Content-Length:");
  start += 15;

  while (*start == ' ' || *start == '\t') {
    start++;
  }
  char *end;
  end = strstr(start, "\r\n");
  if (end)
    *end = 0;

  end = strchr(start, ' ');
  if (end)
    *end = 0;

  return atoi(start);
}

/*
 *
 *  Receives websocket message:
 *  1. Reads websocket header and get content length and message fin
 *  2. Reads websocket payload into a buffer
 *  3. write the actual size written into read_len
 *
 *  Return a pointer to a dynamically allocated buffer.
 *  Needs to be freed by caller function.
 *
 */
char *simple_receive_websocket(SSL *ssl, unsigned long *read_len,
                               int *msg_fin) {
  unsigned long len, content_len, readlen, total_read_bytes;
  char *buf;
  int retval;

  retval = read_websocket_header(ssl, &content_len, msg_fin);
  if (retval < 0) {
    printf("ERROR: not websocket...");
    return NULL;
  }

  buf = malloc(content_len + 1);
  readlen = content_len;
  total_read_bytes = 0;
  do {
    len = SSL_read(ssl, buf + total_read_bytes, readlen);
    total_read_bytes += len;
    readlen -= len;
  } while (len > 0 && total_read_bytes < content_len);

  if (len == 0) {
    fprintf(stdout, "\nconnection closed!\n");
    free(buf);
    return NULL;
  }

  *read_len = content_len;

  return buf;
}

/*
 *
 *  Sends websocket message
 *  1. create websocket header
 *  2. bit mask message and send
 *
 *  Limitations:
 *  -
 *
 */
int send_websocket(SSL *ssl, char *msg, unsigned long msglen, int opcode) {
  // random mask for Websocket Masking
  unsigned int mask;
  getrandom(&mask, sizeof(int), 0);

  // whether payload size is 7 bits (0 extension)
  // or 16 bits (2 byte extension)
  unsigned int extension = 0;

  if (msglen <= 125) {
    extension = 0; // 0 bytes extension of length field
  } else if (msglen <= 65535) {
    extension = 2; // 2 bytes extension of length field
  } else {
    extension = 8; // 8 bytes extension of length field
  }

  unsigned char *ws_frame = malloc(msglen + 14);

  // flags and opcode
  ws_frame[0] = 0x80 + opcode;

  // mask and payload size depending on size group
  if (extension == 0) {
    ws_frame[1] = msglen + 128;
  } else if (extension == 2) {
    ws_frame[1] = 126 + 128;
    ws_frame[2] = ((char *)(&msglen))[1];
    ws_frame[3] = ((char *)(&msglen))[0];
  } else {
    ws_frame[1] = 127 + 128;
    ws_frame[2] = ((char *)(&msglen))[7];
    ws_frame[3] = ((char *)(&msglen))[6];
    ws_frame[4] = ((char *)(&msglen))[5];
    ws_frame[5] = ((char *)(&msglen))[4];
    ws_frame[6] = ((char *)(&msglen))[3];
    ws_frame[7] = ((char *)(&msglen))[2];
    ws_frame[8] = ((char *)(&msglen))[1];
    ws_frame[9] = ((char *)(&msglen))[0];
  }

  // set the mask
  *((unsigned int *)(ws_frame + 2 + extension)) = mask;

  unsigned char *mask_bytes = (unsigned char *)&mask;
  for (unsigned long i = 0; i < msglen; i++) {
    ws_frame[6 + extension + i] = msg[i] ^ mask_bytes[i % 4];
  }

  // send websocket packet to server
  SSL_write(ssl, ws_frame, 6 + extension + msglen);

  free(ws_frame);
  return 0;
}

int send_raw(SSL *ssl, char *data, unsigned long len) {
  return SSL_write(ssl, data, len);
}

int read_raw(SSL *ssl, char *buffer, unsigned long len) {
  return SSL_read(ssl, buffer, len);
}

int init_litesocket() {
  init_openssl();
  return 0;
}

void random_base64(char *buffer, int len) {
  static const unsigned char b64_table[] = {
      'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
      'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
      'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
      'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};

  getrandom(buffer, len, 0);

  for (int i = 0; i < len; i++) {
    buffer[i] = b64_table[buffer[i] & (63)];
  }
}

SSL *ssl_reconnect(SSL *ssl, char *hostname, int hostname_len, char *port,
                   int port_len) {
  int clientfd;
  struct addrinfo hints, *listp;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_INET;       // use ipv4
  hints.ai_flags = AI_NUMERICSERV; // numeric port
  hints.ai_flags |= AI_ADDRCONFIG; // use supported protocols
  getaddrinfo(hostname, port, &hints, &listp);

  if ((clientfd = socket(listp->ai_family, listp->ai_socktype,
                         listp->ai_protocol)) < 0) {
    printf("ERROR: cannot create socket.\n");
    freeaddrinfo(listp);
    return 0;
  }

  if (connect(clientfd, listp->ai_addr, listp->ai_addrlen) < 0) {
    printf("ERROR: cannot connect.\n");
    freeaddrinfo(listp);
    return 0;
  }

  freeaddrinfo(listp);

  SSL_shutdown(ssl);
  SSL_clear(ssl);

  SSL_set_fd(ssl, clientfd);
  if (SSL_connect(ssl) <= 0) {
    printf("Error creating SSL connection.\n");
    return 0;
  }
  // printf("SSL connection using %s\n", SSL_get_cipher(ssl));

  return ssl;
}

SSL *establish_ssl_connection(char *hostname, int hostname_len, char *port,
                              int port_len) {
  int clientfd;
  int errorval;
  struct addrinfo hints, *listp;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_INET;       // use ipv4
  hints.ai_flags = AI_NUMERICSERV; // numeric port
  hints.ai_flags |= AI_ADDRCONFIG; // use supported protocols

  if ((errorval = getaddrinfo(hostname, port, &hints, &listp)) != 0)
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(errorval));

  if ((clientfd = socket(listp->ai_family, listp->ai_socktype,
                         listp->ai_protocol)) < 0) {
    printf("ERROR: cannot create socket.\n");
    freeaddrinfo(listp);
    return 0;
  }

  if (connect(clientfd, listp->ai_addr, listp->ai_addrlen) < 0) {
    printf("ERROR: cannot connect.\n");
    freeaddrinfo(listp);
    return 0;
  }

  freeaddrinfo(listp);

  const SSL_METHOD *meth = TLS_client_method();
  SSL_CTX *ctx = SSL_CTX_new(meth);
  SSL *ssl = SSL_new(ctx);

  // maybe? prevent memory leak?
  SSL_CTX_free(ctx);

  if (!ssl) {
    printf("Error creating SSL.\n");
    return 0;
  }

  SSL_set_fd(ssl, clientfd);
  if (SSL_connect(ssl) <= 0) {
    printf("Error creating SSL connection.\n");
    return 0;
  }
  printf("SSL connection using %s\n", SSL_get_cipher(ssl));

  return ssl;
}

SSL *establish_websocket_connection(char *hostname, int hostname_len,
                                    char *port, int port_len, char *request_uri,
                                    int request_uri_len) {

  // Construct request string
  //------------------------------------
  char websocket_key[25];
  random_base64(websocket_key, 24);
  websocket_key[24] = 0;

  char *request_str = malloc(hostname_len + port_len + request_uri_len + 1000);
  sprintf(request_str,
          "GET %s HTTP/1.1\r\n"
          "Host: %s:%s\r\n"
          "Upgrade: websocket\r\n"
          "Connection: Upgrade\r\n"
          "Sec-WebSocket-Key: %s\r\n"
          "Sec-WebSocket-Version: %d\r\n\r\n",
          request_uri, hostname, port, websocket_key, WEBSOCKET_VERSION);

  //------------------------------------

  SSL *ssl = establish_ssl_connection(hostname, hostname_len, port, port_len);

  SSL_write(ssl, request_str, strlen(request_str));

  char buffer[HTTP_MAX_RESPONSE];
  unsigned long readlen;
  read_http_header(ssl, buffer, HTTP_MAX_RESPONSE, &readlen);

  free(request_str);
  return ssl;
}

// callback will call with null buffer and 0 readlen when the connection ends...
void *threaded_receive_websock(void *ptr) {
  pthread_detach(pthread_self());
  callback_t *callback_data_p = ((callback_t *)ptr);

  char *buffer;
  unsigned long readlen;
  int msgfin;

  int run = 1;
  int trywaitval = -1;
  while (run >= 0) {
    buffer = simple_receive_websocket(callback_data_p->ssl, &readlen, &msgfin);

    trywaitval = sem_trywait(&(callback_data_p->exiter));
    if (trywaitval >= 0) {
      break;
    }

    if (buffer) {
      (callback_data_p->callback)(callback_data_p->ssl, callback_data_p->state,
                                  buffer, readlen);
      free(buffer);
    } else {
      break;
    }
  }

  if (trywaitval < 0) {
    (callback_data_p->callback)(callback_data_p->ssl, callback_data_p->state, 0,
                                0);
  }

  free(ptr);

  fprintf(stderr, "Litesocket listener thread exiting normally.\n");
  return NULL;
}

pthread_t bind_websocket_listener(SSL *ssl, void *state,
                                  callback_func_t callback, sem_t **exiter) {
  pthread_t tid;

  callback_t *callback_data = malloc(sizeof(callback_t));
  callback_data->callback = callback;
  callback_data->ssl = ssl;
  callback_data->state = state;
  *exiter = &(callback_data->exiter);
  sem_init(&(callback_data->exiter), 0, 0);

  pthread_create(&tid, NULL, threaded_receive_websock, callback_data);

  return tid;
}