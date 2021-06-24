#include "litesocket.h"


void websocket_listen_callback(SSL *ssl, char *msg, unsigned long msg_len){
  write(STDOUT_FILENO, msg, msg_len);
  write(STDOUT_FILENO, "\n", 1);
}

int main() {
  SSL *ssl = establish_websocket_connection("gateway.discord.gg", 9, "443", 3,
                                            "/?v=9&encoding=json", 1);

  pthread_t tid = bind_websocket_listener(ssl, NULL, websocket_listen_callback);

  char buffer[4096];
  buffer[0] = 0;
  buffer[4095] = 0;

  fgets(buffer, 4095, stdin);
  while(!strstr(buffer, "quit")){
    printf("\n\n");
    send_websocket(ssl, buffer, strlen(buffer), WEBSOCKET_OPCODE_MSG);
    fgets(buffer, 4095, stdin);
  }

  pthread_cancel(tid);
  disconnect_and_free_ssl(ssl);
}