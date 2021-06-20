#include "litesocket.h"


void websocket_listen_callback(char *msg, unsigned long msg_len){
  write(STDOUT_FILENO, msg, msg_len);
  printf("\n");
}

int main() {
  SSL *ssl = establish_websocket_connection("gateway.discord.gg", 9, "443", 3,
                                            "/?v=9&encoding=json", 1);

  bind_websocket_listener(ssl, websocket_listen_callback);

  char buffer[4096];
  while(1){
    fgets(buffer, 4096, stdin);
    printf("\n\n");
    fflush(stdout);
    send_websocket(ssl, buffer, strlen(buffer), WEBSOCKET_OPCODE_MSG);
  }

  disconnect_and_free_ssl(ssl);
}