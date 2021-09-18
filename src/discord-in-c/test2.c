#include "sbuf.structs.h"
#include "discord.h"
#include "media.h"
#include "discord.structs.h"
#include "media.structs.h"
#include "cJSON.h"

char *bottoken;
char botprefix[10];

//media_player_t *media;

sem_t play_cmd_mutex;

void escape_http_newline(char *input, long unsigned in_size, char *output, long unsigned out_size){
  char *desc_after   = input;
  char *desc_before  = input;
  char *text_inputer = output;

  long unsigned cumulative_size = 0;
  while(desc_before < input + in_size){
    desc_after = strchr(desc_before, '\n');

    if(desc_after){
      *desc_after = 0;
    }

    strncpy(text_inputer, desc_before, out_size - cumulative_size);
    cumulative_size = cumulative_size + strlen(desc_before);

    if(cumulative_size + 2 >= out_size) break;

    if(desc_after){
      output[cumulative_size] = '\\';
      output[cumulative_size + 1] = 'n';
      output[cumulative_size + 2] = 0;
      cumulative_size = cumulative_size + 2;
      desc_before = desc_after + 1;
      text_inputer = output + cumulative_size;
    }else{
      break;
    }
  }

  output[out_size - 1] = 0;
}

void escape_http_doublequote(char *input, long unsigned in_size, char *output, long unsigned out_size){
  char *desc_after   = input;
  char *desc_before  = input;
  char *text_inputer = output;

  long unsigned cumulative_size = 0;
  while(desc_before < input + in_size){
    desc_after = strchr(desc_before, '"');

    if(desc_after){
      *desc_after = 0;
    }

    strncpy(text_inputer, desc_before, out_size - cumulative_size);
    cumulative_size = cumulative_size + strlen(desc_before);

    if(cumulative_size + 2 >= out_size) break;

    if(desc_after){
      output[cumulative_size] = '\\';
      output[cumulative_size + 1] = '"';
      output[cumulative_size + 2] = 0;
      cumulative_size = cumulative_size + 2;
      desc_before = desc_after + 1;
      text_inputer = output + cumulative_size;
    }else{
      break;
    }
  }

  output[out_size - 1] = 0;
}

void fix_string_ending(char *str){
  int length = strlen(str);
  unsigned char *working = ((unsigned char *)str) + length - 1;

  while(working != ((unsigned char *)str)){
    if(((*working) & 0xC0) != 0x80){
      break;
    }

    working--;
  }

  if(working != ((unsigned char *)str) + length - 1)
    *working = 0;
}

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

  fprintf(stdout, "\n\n %s %s %s %s \n\n", secret_key, ssrc, ip, port);

  modify_player(vgt->media, secret_key, ssrc,
                            ip, port, vgt->voice_udp_sockfd,
                            "audiotmpfile.out", vgt);
}

void on_message(void *state, char *msg, unsigned long msg_len) {
  write(STDOUT_FILENO, msg, msg_len);
  write(STDOUT_FILENO, "yayay \n", strlen("yayay \n"));
}

int counter1 = 0;

struct play_cmd_obj{
  int ret;
  voice_gateway_t *vgt;
  discord_t *dis;
  user_vc_obj uobj;
  char *content;
};

void *threaded_play_cmd(void *ptr){
  pthread_detach(pthread_self());

  struct play_cmd_obj *pobj = ptr;
  char *og_content = pobj->content;

  sem_wait(&(play_cmd_mutex));
      if(!(pobj->ret)){
        fprintf(stdout, "\n\n CONNECTING VOICE >>>>>>>>>> %s\n%s\n\n", pobj->uobj.guild_id, pobj->uobj.vc_id);

        pobj->vgt = connect_voice_gateway(pobj->dis, pobj->uobj.guild_id, pobj->uobj.vc_id,
                        on_message, on_reconnect_voice, 0);

        fprintf(stdout, "\n\n DONE CONNECTING VOICE \n\n");

        char secret_key[1000];
        char ip[100];
        char port[100];
        char *ssrc = "66666";

        sm_get(pobj->vgt->data_dictionary, DISCORD_VOICE_IP, ip, 100);
        sm_get(pobj->vgt->data_dictionary, DISCORD_VOICE_PORT, port, 100);
        sm_get(pobj->vgt->data_dictionary, DISCORD_VOICE_SECRET_KEY, secret_key, 1000);

        fprintf(stdout, "\n\nstarting media player.......\n\n");

        char filename[128] = { 0 };
        snprintf(filename, sizeof(filename) - 1, "audiotmpfile.%s.out", pobj->uobj.guild_id);

        fprintf(stdout, "setting up media player thread...\n");
        pobj->vgt->media = start_player(secret_key, ssrc,
                                  ip, port, pobj->vgt->voice_udp_sockfd,
                                  filename, pobj->vgt);
      }

      write(STDOUT_FILENO, "\n", 1);
      write(STDOUT_FILENO, pobj->content, strlen(pobj->content));
      write(STDOUT_FILENO, "\n", 1);
      pobj->content += 3;

      fprintf(stdout, "Queueing song...\n");
      if(!strncasecmp(pobj->content, "https://", 8) && 1){
        insert_queue_ydl_query(pobj->vgt->media, pobj->content);
      }else{
        char youtube_dl_search_txt[2048];
        snprintf(youtube_dl_search_txt, 2048, "ytsearch1:%s", pobj->content);
        insert_queue_ydl_query(pobj->vgt->media, youtube_dl_search_txt);
      }
    sem_post(&(play_cmd_mutex));

  free(og_content);
  free(pobj);

  return NULL;
}

void get_queue_callback(void *value, int len, void *state, int pos, int start, int end){
  char **array = state;
  youtube_page_object_t *ytobj = value;

  array[pos - start] = malloc(len);

  char text3[sizeof(ytobj->title)];
  char text4[sizeof(ytobj->title)];

  escape_http_newline(ytobj->title, sizeof(ytobj->title), text3, sizeof(ytobj->title));
  escape_http_doublequote(text3, sizeof(text3), text4, sizeof(text4));
  fix_string_ending(text4);

  memcpy(array[pos - start], text4, len);
}

void add_user_vc_record(discord_t *discord, char *msg, int msg_len){
  cJSON *cjs = cJSON_ParseWithLength(msg, msg_len);
  cJSON *d_cjs = cJSON_GetObjectItem(cjs, "d");
  cJSON *gid_cjs = cJSON_GetObjectItem(d_cjs, "id");

  if (gid_cjs == NULL)
  {
    fprintf(stdout, "\nNULL obj\n");
      const char *error_ptr = cJSON_GetErrorPtr();
      if (error_ptr != NULL)
      {
          fprintf(stderr, "Error before: %s\n", error_ptr);
      }
  }
  fprintf(stdout, "\ngid:%s\n", gid_cjs->valuestring);

  cJSON *voice_states = cJSON_GetObjectItem(d_cjs, "voice_states");
  voice_states = voice_states->child;

  while(voice_states){
    cJSON *uid, *cid;
    uid = cJSON_GetObjectItem(voice_states, "user_id");
    cid = cJSON_GetObjectItem(voice_states, "channel_id");

    user_vc_obj uobj;
    strcpy(uobj.user_id, uid->valuestring);
    strcpy(uobj.vc_id, cid->valuestring);
    strcpy(uobj.guild_id, gid_cjs->valuestring);

    sm_put(discord->user_vc_map, uobj.user_id, (char *)&uobj, sizeof(uobj));

    voice_states = voice_states->next;
  }
  fprintf(stdout, "done parsing...\n");

  cJSON_Delete(cjs);
}


void actually_do_shit(void *state, char *msg, unsigned long msg_len) {
  discord_t *dis = state;

  write(STDOUT_FILENO, msg, msg_len);
  write(STDOUT_FILENO, "WOW WOW WOW\n", strlen("WOW WOW WOW\n"));

  if (strcasestr(msg, "\"GUILD_CREATE\"")){
    add_user_vc_record(dis, msg, msg_len);
    return;
  }

  if (strcasestr(msg, "\"MESSAGE_CREATE\"")){
    char *content = strcasestr(msg, "content") + 10;
    if ((content[0] == botprefix[0]) && !strncasecmp(content+1, "prefix", 6)){
      botprefix[0] = *(content + 8);
      return;
    }

    char *userid = strcasestr(msg, DISCORD_GATEWAY_VOICE_USERNAME);
    char *userid_cp = 0;
    if(userid){
      userid = strcasestr(userid, DISCORD_GATEWAY_VOICE_USER_ID);
      userid += 6;
      char *end = strchr(userid, '"');
      *end = 0;
      userid_cp = malloc(strlen(userid) + 1);
      strcpy(userid_cp, userid);
      *end = '"';
      userid = userid_cp;
      fprintf(stdout, "\n\nDETECTED USER: %s\n\n", userid);
    }

    char *textchannelid_nw = 0;
    char *textchannelid = strcasestr(msg, DISCORD_GATEWAY_MSG_CHANNEL_ID);;
    if(textchannelid){
      textchannelid += 14;
      char *end = strchr(textchannelid, '"');
      *end = 0;

      textchannelid_nw = malloc(strlen(textchannelid) + 1);
      strcpy(textchannelid_nw, textchannelid);

      *end = '"';
      textchannelid = textchannelid_nw;

      fprintf(stdout, "\n\nTEXT CHANNEL: %s\n\n", textchannelid);
    }

    user_vc_obj uobj;
    int has_user = sm_get(dis->user_vc_map, userid, (char *)&uobj, sizeof(uobj));
    fprintf(stdout, "\n\nUSER IN CHANNEL: %s, guild: %s\n\n", uobj.vc_id, uobj.guild_id);

    msg[msg_len] = 0;
    while(*msg == ' '){
      msg++;
    }

    if (has_user) {
      char *end = strchr(content, ',') - 1;
      *end = 0;

      voice_gateway_t *vgt = 0;
      int ret = 1;
      if (!strncasecmp(content, botprefix, 1)){
        sm_get(dis->voice_gateway_map, uobj.guild_id, (char *)&vgt,
              sizeof(void *));
        if(!vgt){
          ret = 0;
        }
      }

      fprintf(stdout, "\n%s %d\n", content, ret);

      if((content[0] == botprefix[0]) && !strncasecmp(content+1, "leave", 5) && ret
          && vgt && vgt->media && vgt->media->initialized){
        
        sem_wait(&(vgt->media->insert_song_mutex));
        fprintf(stdout, "\nLEAVING leaving... LEAVING\n");
        sem_post(&(vgt->media->quitter));
        youtube_page_object_t yobj = { 0 };
        sbuf_insert_front_value((&(vgt->media->song_queue)), &yobj, sizeof(yobj));
        sem_post(&(vgt->media->skipper));

        fprintf(stdout, "\nLEAVING leaving... LEAVING\n");
        char *ptr = 0;
        sm_put(dis->voice_gateway_map, uobj.guild_id, (char *)&ptr,
              sizeof(void *));
        send_websocket(vgt->voice_ssl,
            "request close",
            strlen("request close"),
            8);

        char msg[2000];
        snprintf(msg, 2000,
              DISCORD_GATEWAY_VOICE_LEAVE, uobj.guild_id);
        sem_wait(&(dis->gateway_writer_mutex));
        send_websocket(dis->gateway_ssl, msg, strlen(msg), WEBSOCKET_OPCODE_MSG);
        sem_post(&(dis->gateway_writer_mutex));
        
        fprintf(stdout, "\nLEAVING leaving... LEAVING\n");
        free_voice_gateway(vgt);
        fprintf(stdout, "\nLEAVING leaving... LEAVING\n");
      }else if ((content[0] == botprefix[0]) && !strncasecmp(content+1, "skip", 4) && ret){
        
        sem_post(&(vgt->media->skipper));

      } else if ((content[0] == botprefix[0]) && !strncasecmp(content+1, "desc", 4) && ret){

        fprintf(stdout, "\ntrying to send msg...\n");
        
        ssl_reconnect(dis->https_api_ssl, DISCORD_HOST, strlen(DISCORD_HOST),
                                            DISCORD_PORT, strlen(DISCORD_PORT));

        char message[9500];
        
        if(vgt->media && vgt->media->playing){
          youtube_page_object_t ytpobj;
          sbuf_peek_end_value(&(vgt->media->song_queue), &(ytpobj), sizeof(ytpobj), 0);

          char text[sizeof(ytpobj.description)];
          char text2[sizeof(ytpobj.description)];

          escape_http_newline(ytpobj.description, sizeof(ytpobj.description), text, sizeof(ytpobj.description));

          fprintf(stdout, "\n%s\n\n\n", text);
          fflush(stdout);

          escape_http_doublequote(text, sizeof(text), text2, sizeof(text2));

          fix_string_ending(text2);

          char text3[sizeof(ytpobj.title)];
          char text4[sizeof(ytpobj.title)];

          escape_http_newline(ytpobj.title, sizeof(ytpobj.title), text3, sizeof(ytpobj.title));
          escape_http_doublequote(text3, sizeof(text3), text4, sizeof(text4));
          fix_string_ending(text4);

          snprintf(message, 9500, DISCORD_API_POST_BODY_MSG_EMBED, "Now Playing:", text4, ytpobj.link, text2);
          //snprintf(message, 9500, DISCORD_API_POST_BODY_MSG_SIMPLE, "Not currently playing a song!");
        }
        else{
          snprintf(message, 9500, DISCORD_API_POST_BODY_MSG_SIMPLE, "Not currently playing a song!");
        }

        char header[2000];
        snprintf(header, 2000, DISCORD_API_POST_MSG, textchannelid, bottoken, (int)strlen(message));
        char buffer[13000];
        snprintf(buffer, 13000, "%s\r\n\r\n%s\r\n\r\n", header, message);
        fprintf(stdout, buffer);
        send_raw(dis->https_api_ssl, buffer,
            strlen(buffer));

      }else if ((content[0] == botprefix[0]) && !strncasecmp(content+1, "np", 2) && ret){

        fprintf(stdout, "\ntrying to send msg...\n");
        ssl_reconnect(dis->https_api_ssl, DISCORD_HOST, strlen(DISCORD_HOST),
                                            DISCORD_PORT, strlen(DISCORD_PORT));
        char message[9500];
        
        if(vgt->media && vgt->media->playing){
          youtube_page_object_t ytpobj;
          sbuf_peek_end_value(&(vgt->media->song_queue), &(ytpobj), sizeof(ytpobj), 0);

          char text3[sizeof(ytpobj.title)];
          char text4[sizeof(ytpobj.title)];

          escape_http_newline(ytpobj.title, sizeof(ytpobj.title), text3, sizeof(ytpobj.title));
          escape_http_doublequote(text3, sizeof(text3), text4, sizeof(text4));
          fix_string_ending(text4);


          struct timespec now;
          clock_gettime(CLOCK_REALTIME, &now);
          long lapse = now.tv_sec - vgt->media->song_start_time.tv_sec;

          fprintf(stdout, "lapse: %ld\n", lapse);
          
          #define barsize 40
          char bar[barsize] = {0};
          long progress = (barsize-1) * lapse / (ytpobj.length_in_seconds);
          for(int i = 0; i < barsize; i++){
            if(i < progress)
              strcat(bar, "#");
            else
              strcat(bar, "-");
          }

          fprintf(stdout, "bar: %s\n", bar);

          char time_str[200] = {0};
          snprintf(time_str, sizeof(time_str), "%ld:%ld:%ld [%s] %s", lapse / 3600, (lapse / 60) % 60, lapse % 60, bar, ytpobj.duration);


          snprintf(message, 9500, DISCORD_API_POST_BODY_MSG_EMBED, "Now Playing:", text4, ytpobj.link, time_str);
        }
        else{
          snprintf(message, 9500, DISCORD_API_POST_BODY_MSG_SIMPLE, "Not currently playing a song!");
        }
        char header[2000];
        snprintf(header, 2000, DISCORD_API_POST_MSG, textchannelid, bottoken, (int)strlen(message));
        char buffer[13000];
        snprintf(buffer, 13000, "%s\r\n\r\n%s\r\n\r\n", header, message);
        fprintf(stdout, buffer);
        send_raw(dis->https_api_ssl, buffer,
            strlen(buffer));

      }else if ((content[0] == botprefix[0]) && !strncasecmp(content + 1, "queue", 5) && ret){

        fprintf(stdout, "\ntrying to send msg...\n");
        ssl_reconnect(dis->https_api_ssl, DISCORD_HOST, strlen(DISCORD_HOST),
                                            DISCORD_PORT, strlen(DISCORD_PORT));
        char message[9500];
        
        if(vgt->media && vgt->media->playing){
          char *(title_arr[5]) = { 0 };
          sbuf_iterate(&(vgt->media->song_queue), get_queue_callback, title_arr, 0, 4);

          char inner_message[5000];
          snprintf(inner_message, 5000, "1. %s\\n2. %s\\n3. %s\\n4. %s\\n5. %s\\n",
                  title_arr[0], title_arr[1], title_arr[2], title_arr[3], title_arr[4]);

          for(int i = 0; i < 5; i++){
            free(title_arr[i]);
          }

          snprintf(message, 9500, DISCORD_API_POST_BODY_MSG_EMBED, "Song Queue:", "Up next on the playlist...", inner_message, "To see more songs, use \\\"queue [page number]\\\"");
        }
        else{
          snprintf(message, 9500, DISCORD_API_POST_BODY_MSG_SIMPLE, "No song playing!");
        }
        char header[2000];
        snprintf(header, 2000, DISCORD_API_POST_MSG, textchannelid, bottoken, (int)strlen(message));
        char buffer[13000];
        snprintf(buffer, 13000, "%s\r\n\r\n%s\r\n\r\n", header, message);
        fprintf(stdout, buffer);
        send_raw(dis->https_api_ssl, buffer,
            strlen(buffer));

      }else if ((content[0] == botprefix[0]) && !strncasecmp(content+1, "p ", 2)) {
        struct play_cmd_obj *pobj = malloc(sizeof(struct play_cmd_obj));
        pobj->dis = dis;
        pobj->ret = ret;
        pobj->vgt = vgt;
        pobj->uobj = uobj;

        pobj->content = malloc(strlen(content) + 1);
        strcpy(pobj->content, content);

        pthread_t tid;
        pthread_create(&tid, NULL, threaded_play_cmd, pobj);
      }
    }

    if(userid_cp){
      free(userid_cp);
    }
    if(textchannelid_nw){
      free(textchannelid_nw);
    }

  }
  
}

int main(int argc, char **argv) {
  strcpy(botprefix, BOT_PREFIX);
  sem_init(&(play_cmd_mutex), 0, 1);

  if (argc > 1) {
    bottoken = argv[1];
  } else {
    bottoken = getenv("TOKEN");
  }
  discord_t *discord = init_discord(bottoken, "641");

  fprintf(stdout, "Token: %s\n", BOT_PREFIX"");

  char buf[100];
  sm_get(discord->data_dictionary, DISCORD_HOSTNAME_KEY, buf, 100);
  printf("%s\n", buf);

  set_gateway_callback(discord, actually_do_shit);
  connect_gateway(discord);

  while (1)
    sleep(100);

  free_discord(discord);

  printf("exiting cleanly\n");
}
