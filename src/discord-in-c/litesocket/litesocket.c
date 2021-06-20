#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>

#include "csapp.h"

/* Helper functions for websocket connections */

/*
 *
 *  Inits openssl
 */
void init_openssl_2() {
  SSL_library_init();
  SSL_load_error_strings();
}

/*
 *
 *  Reads HTTP header
 */
int read_http_header(SSL *ssl, char *headerbuf) {
  char *hbp = headerbuf;

  char four[4];
  four[0] = 0;
  four[1] = 0;
  four[2] = 0;
  four[3] = 0;

  char buf[10];
  int len;

  do {
    if((len = SSL_read(ssl, buf, 1)) < 0){
        return -1;
    }

    *hbp = buf[0];
    hbp++;

    four[0] = four[1];
    four[1] = four[2];
    four[2] = four[3];
    four[3] = buf[0];

  } while (len > 0 && !(four[0] == '\r' && four[1] == '\n' && four[2] == '\r' &&
                        four[3] == '\n'));
}


/*
 *
 *  Reads websocket header and writes msg_len and msg_fin:
 *  msg_len is the length of payload
 *  msg_fin is the fin bit in websocket
 */
int read_websocket_header(SSL *ssl, unsigned long *msg_len, int *msg_fin) {
  unsigned char frameHeader[16];
  unsigned long plen;

  if(SSL_read(ssl, frameHeader, 2) < 0){
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
  } else if (plen == 127){
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

/*
 *
 *  Receives websocket message:
 *  1. Reads websocket header and get content length and message fin
 *  2. Reads websocket payload into a buffer
 *  3. copy into the buffer and write the actual size written into buffer
 * 
 */
int simple_receive_websocket(SSL *ssl, char *read_buffer, unsigned long buffer_len, unsigned long *read_len, int *msg_fin) {
  unsigned long len, content_len, readlen, total_read_bytes = 0, copied_len;
  char *buf;

  read_websocket_header(ssl, &content_len, msg_fin);
  if (content_len < 0) {
    printf("ERROR: not websocket...");
    return -1;
  }
  
  buf = malloc(content_len);
  readlen = content_len;
  do {
    len = SSL_read(ssl, buf, readlen);
    total_read_bytes += len;
    readlen -= len;
  } while (len > 0 && total_read_bytes < content_len);

  if (len == 0) {
    printf("connection closed!");
    free(buf);
    return -2;
  }

  copied_len = CALC_MIN(content_len, buffer_len);
  memcpy(read_buffer, buf, copied_len);
  *read_len = copied_len;

  free(buf);
  return 1;
}