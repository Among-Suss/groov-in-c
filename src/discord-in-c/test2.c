#include "sbuf.structs.h"
#include "discord.h"
#include "media.h"
#include "discord.structs.h"
#include "media.structs.h"
#include "youtube/youtube-func.h"


char *serverid = "807911659078680576";
char *channelid = "857087599557607466";
char *channel2 = "807911659078680580";
char *textchannelid = "816227774531502111";
char *bottoken;

media_player_t *media;

void on_reconnect_voice(void *state, char *msg, unsigned long msg_len) {
  fprintf(stdout, "\n\non_reconnect_voice RECONNECTING MEDIA\n\n");

  voice_gateway_t *vgt = (voice_gateway_t *)state;

  char secret_key[1000];
  char ip[100];
  char port[100];
  char *ssrc = "66666";

  sm_get(vgt->data_dictionary, DISCORD_VOICE_IP, ip, 100);
  sm_get(vgt->data_dictionary, DISCORD_VOICE_PORT, port, 100);
  sm_get(vgt->data_dictionary, DISCORD_VOICE_SECRET_KEY, secret_key, 1000);

  modify_player(media, secret_key, ssrc,
                            ip, port, vgt->voice_udp_sockfd,
                            "audiotmpfile.out", vgt);
}

void on_message(void *state, char *msg, unsigned long msg_len) {
  write(STDOUT_FILENO, msg, msg_len);
  write(STDOUT_FILENO, "yayay \n", strlen("yayay \n"));
}

int counter1 = 0;


void actually_do_shit(void *state, char *msg, unsigned long msg_len) {
  discord_t *dis = state;

  write(STDOUT_FILENO, msg, msg_len);
  write(STDOUT_FILENO, "WOW WOW WOW\n", strlen("WOW WOW WOW\n"));

  msg[msg_len] = 0;
  while(*msg == ' '){
    msg++;
  }

  if (strcasestr(msg, "MESSAGE_CREATE")) {
    char *content = strcasestr(msg, "content") + 10;
    char *end = strchr(content, ',') - 1;
    *end = 0;

    if (!strncasecmp(content, "=skip", 5)){
      
      sem_post(&(media->skipper));

    } else if (!strncasecmp(content, "=np", 3)){

      fprintf(stdout, "\ntrying to send msg...\n");
      
      ssl_reconnect(dis->https_api_ssl, DISCORD_HOST, strlen(DISCORD_HOST),
                                          DISCORD_PORT, strlen(DISCORD_PORT));

      char message[3000];
      if(media && media->playing)
        sprintf(message, DISCORD_API_POST_BODY_MSG_SIMPLE, media->current_url);
      else
        sprintf(message, DISCORD_API_POST_BODY_MSG_SIMPLE, "Not currently playing a song!");

      char header[2000];
      sprintf(header, DISCORD_API_POST_MSG, textchannelid, bottoken, (int)strlen(message));
      char buffer[6000];
      sprintf(buffer, "%s\r\n\r\n%s\r\n\r\n", header, message);
      fprintf(stdout, buffer);
      send_raw(dis->https_api_ssl, buffer,
           strlen(buffer));

    }else if (!strncasecmp(content, "=p ", 3)) {
      write(STDOUT_FILENO, "\n", 1);
      write(STDOUT_FILENO, content, strlen(content));
      write(STDOUT_FILENO, "\n", 1);
      content += 3;

      voice_gateway_t *vgt;
      sm_get(dis->voice_gateway_map, serverid, (char *)&vgt,
             sizeof(void *));
      //printf("%d\n", vgt);
      // DANGEROUS
      /*
      send_websocket(
          vgt->voice_ssl,
          "{\"op\":5,\"d\":{\"speaking\":5,\"delay\":0,\"ssrc\":66666}}",
          strlen(
              "{\"op\":5,\"d\":{\"speaking\":5,\"delay\":0,\"ssrc\":66666}}"),
          1);
      */
      //printf("TOUCH!\n");

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

      char ufid[1000];
      memset(ufid, 0, 1000);
      snprintf(ufid, 1000, "AUDIOTMP.%d%s", counter1, ".out");
      //counter1++;

      //play_youtube_in_thread(argv[5], argv[4], argv[3], argv[1], argv[2], vgt->voice_udp_sockfd, ufid);

      if(!strncasecmp(content, "https://", 8)){
        sbuf_insert_front_value((&(media->song_queue)), content, strlen(content) + 1);
      }else{
        char token[YOUTUBE_TOKEN_SIZE];
        search_youtube_for_link(content, token);
        sbuf_insert_front_value((&(media->song_queue)), token, strlen(token) + 1);
      }

      

      /*
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

        //char *arge[2];
        //arge[0] = "/usr/bin/ls";
        //arge[1] = 0;
        //execv(arge[0], arge);
        //printf("fail?\n");

        int x = execv(argv[0], argv);
        printf("execve failed: %d\n\n", x);
      }
      */
    }
  }
}

int main(int argc, char **argv) {
  bottoken = argv[1];
  discord_t *discord = init_discord(argv[1]);

  char buf[100];
  sm_get(discord->data_dictionary, DISCORD_HOSTNAME_KEY, buf, 100);
  printf("%s\n", buf);

  set_gateway_callback(discord, actually_do_shit);
  connect_gateway(discord, "643");

  voice_gateway_t *vgt = connect_voice_gateway(discord, serverid, channelid,
                        on_message, on_reconnect_voice);

  char secret_key[1000];
  char ip[100];
  char port[100];
  char *ssrc = "66666";

  sm_get(vgt->data_dictionary, DISCORD_VOICE_IP, ip, 100);
  sm_get(vgt->data_dictionary, DISCORD_VOICE_PORT, port, 100);
  sm_get(vgt->data_dictionary, DISCORD_VOICE_SECRET_KEY, secret_key, 1000);

  media = start_player(secret_key, ssrc,
                            ip, port, vgt->voice_udp_sockfd,
                            "audiotmpfile.out", vgt);

  while (1)
    sleep(100);

  free_discord(discord);

  printf("exiting cleanly\n");
}
