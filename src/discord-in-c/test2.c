#include "discord.h"

void on_message(void *state, char *msg, unsigned long msg_len) {
  write(STDOUT_FILENO, msg, msg_len);
  write(STDOUT_FILENO, "yayay \n", strlen("yayay \n"));
}

void actually_do_shit(void *state, char *msg, unsigned long msg_len) {
  discord_t *dis = state;

  write(STDOUT_FILENO, msg, msg_len);
  write(STDOUT_FILENO, "\n", 1);

  if (strcasestr(msg, "MESSAGE_CREATE")) {
    char *content = strcasestr(msg, "content") + 10;
    char *end = strchr(content, ',') - 1;
    *end = 0;

    if (!strncasecmp(content, "=p ", 3)) {
      write(STDOUT_FILENO, "\n", 1);
      write(STDOUT_FILENO, content, strlen(content));
      write(STDOUT_FILENO, "\n", 1);
      content += 3;

      voice_gateway_t *vgt;
      sm_get(dis->voice_gateway_map, "807911659078680576", (char *)&vgt,
             sizeof(void *));
      //printf("%d\n", vgt);
      send_websocket(
          vgt->voice_ssl,
          "{\"op\":5,\"d\":{\"speaking\":5,\"delay\":0,\"ssrc\":66666}}",
          strlen(
              "{\"op\":5,\"d\":{\"speaking\":5,\"delay\":0,\"ssrc\":66666}}"),
          1);
      //printf("TOUCH!\n");
      if (fork() == 0) {
        char *argv[7];
        argv[0] = "./udprtp";
        char a[100];
        argv[1] = a;
        char b[100];
        argv[2] = b;
        argv[3] = "66666";
        char c[1000];
        argv[4] = c;
        char d[1000];
        argv[5] = d;
        argv[6] = 0;

        strcpy(argv[5], content);

        sm_get(vgt->data_dictionary, DISCORD_VOICE_IP, argv[1], 100);
        sm_get(vgt->data_dictionary, DISCORD_VOICE_PORT, argv[2], 100);
        sm_get(vgt->data_dictionary, DISCORD_VOICE_SECRET_KEY, argv[4], 1000);

        printf("\n\n%s\n%s\n%s\n%s\n%s\n", argv[1], argv[2], argv[3], argv[4],
               argv[5]);

        char *arge[2];
        arge[0] = "/usr/bin/ls";
        arge[1] = 0;
        //execv(arge[0], arge);
        //printf("fail?\n");

        int x = execv(argv[0], argv);
        printf("execve failed: %d\n\n", x);
      }
    }
  }
}

int main(int argc, char **argv) {
  discord_t *discord = init_discord(argv[1]);

  char buf[100];
  sm_get(discord->data_dictionary, DISCORD_HOSTNAME_KEY, buf, 100);
  printf("%s\n", buf);

  set_gateway_callback(discord, actually_do_shit);
  connect_gateway(discord, "643");

  connect_voice_gateway(discord, "807911659078680576", "857087599557607466",
                        on_message);

  while (1)
    sleep(100);

  free_discord(discord);

  printf("exiting cleanly\n");
}