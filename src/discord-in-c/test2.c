#include "cJSON.h"
#include "sbuf.structs.h"
#include "discord.h"
#include "discord.structs.h"
#include "media.h"
#include "media.structs.h"

#define DEFAULT_BOT_NAME "groov-in-c"
char *botname;
char *bottoken;

sem_t play_cmd_mutex;

/* Simple sending message to discord
 *
 *
 *
 */
void simple_send_msg(discord_t *dis, char *text, char *textchannelid) {
  ssl_reconnect(dis->https_api_ssl, DISCORD_HOST, strlen(DISCORD_HOST),
                DISCORD_PORT, strlen(DISCORD_PORT));
  char message[4000];
  snprintf(message, 4000, DISCORD_API_POST_BODY_MSG_SIMPLE, text);
  char header[2000];
  snprintf(header, 2000, DISCORD_API_POST_MSG, textchannelid, bottoken,
           (int)strlen(message));
  char buffer[5000];
  snprintf(buffer, 5000, "%s\r\n\r\n%s\r\n\r\n", header, message);
  send_raw(dis->https_api_ssl, buffer, strlen(buffer));
}

void send_typing_indicator(discord_t *dis, char *textchannelid){
  ssl_reconnect(dis->https_api_ssl, DISCORD_HOST, strlen(DISCORD_HOST),
                DISCORD_PORT, strlen(DISCORD_PORT));
  char header[1000];
  snprintf(header, sizeof(header), DISCORD_API_POST_TYPING, textchannelid, bottoken);
  char buffer[1100];
  snprintf(buffer, sizeof(buffer), "%s\r\n\r\n\r\n\r\n", header);
  send_raw(dis->https_api_ssl, buffer, strlen(buffer));
}

/* Helper Functions for sending strings in JSON
 *
 *  - these functions are necessary to escape out json string properly
 *
 */
void escape_http_newline(char *input, long unsigned in_size, char *output,
                         long unsigned out_size) {
  char *desc_after = input;
  char *desc_before = input;
  char *text_inputer = output;

  long unsigned cumulative_size = 0;
  while (desc_before < input + in_size) {
    desc_after = strchr(desc_before, '\n');

    if (desc_after) {
      *desc_after = 0;
    }

    strncpy(text_inputer, desc_before, out_size - cumulative_size);
    cumulative_size = cumulative_size + strlen(desc_before);

    if (cumulative_size + 2 >= out_size)
      break;

    if (desc_after) {
      output[cumulative_size] = '\\';
      output[cumulative_size + 1] = 'n';
      output[cumulative_size + 2] = 0;
      cumulative_size = cumulative_size + 2;
      desc_before = desc_after + 1;
      text_inputer = output + cumulative_size;
    } else {
      break;
    }
  }

  output[out_size - 1] = 0;
}
void escape_http_doublequote(char *input, long unsigned in_size, char *output,
                             long unsigned out_size) {
  char *desc_after = input;
  char *desc_before = input;
  char *text_inputer = output;

  long unsigned cumulative_size = 0;
  while (desc_before < input + in_size) {
    desc_after = strchr(desc_before, '"');

    if (desc_after) {
      *desc_after = 0;
    }

    strncpy(text_inputer, desc_before, out_size - cumulative_size);
    cumulative_size = cumulative_size + strlen(desc_before);

    if (cumulative_size + 2 >= out_size)
      break;

    if (desc_after) {
      output[cumulative_size] = '\\';
      output[cumulative_size + 1] = '"';
      output[cumulative_size + 2] = 0;
      cumulative_size = cumulative_size + 2;
      desc_before = desc_after + 1;
      text_inputer = output + cumulative_size;
    } else {
      break;
    }
  }

  output[out_size - 1] = 0;
}
void fix_string_ending(char *str) {
  int length = strlen(str);
  unsigned char *working = ((unsigned char *)str) + length - 1;

  while (working != ((unsigned char *)str)) {
    if (((*working) & 0xC0) != 0x80) {
      break;
    }

    working--;
  }

  if (working != ((unsigned char *)str) + length - 1)
    *working = 0;
}

/*  Voice reconnect callback
 *
 *  This function is called when bot needs to reconnect.
 *  Use this function to reconnect the MEDIA PLAYER
 */
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

  modify_player(vgt->media, secret_key, ssrc, ip, port, vgt->voice_udp_sockfd,
                "audiotmpfile.out", vgt);
}

/* Voice Gateway callback.
 *
 * Not much to do here
 *
 */
void on_message(void *state, char *msg, unsigned long msg_len) {
  write(STDOUT_FILENO, msg, msg_len);
  write(STDOUT_FILENO, "yayay \n", strlen("yayay \n"));
}

/* Connect bot/queue song in a thread.
 *
 * needs threaded model because connection needs to continue receiving websocket
 * while connecting...
 *
 */
struct play_cmd_obj {
  voice_gateway_t *vgt;
  discord_t *dis;
  user_vc_obj uobj;
  char *content;
  char *textchannelid;
};
void *threaded_play_cmd(void *ptr) {
  pthread_detach(pthread_self());

  struct play_cmd_obj *pobj = ptr;
  char *og_content = pobj->content;

  send_typing_indicator(pobj->dis, pobj->textchannelid);

  sem_wait(&(play_cmd_mutex));
  if (!(pobj->vgt)) {
    fprintf(stdout, "\n\n CONNECTING VOICE >>>>>>>>>> %s\n%s\n\n",
            pobj->uobj.guild_id, pobj->uobj.vc_id);

    pobj->vgt =
        connect_voice_gateway(pobj->dis, pobj->uobj.guild_id, pobj->uobj.vc_id,
                              on_message, on_reconnect_voice, 0);

    if (!(pobj->vgt)) {
      simple_send_msg(pobj->dis,
                      "Failed to connect to voice channel. Make sure the bot "
                      "has permission to view voice channel.",
                      pobj->textchannelid);
      goto CLEANUP;
    }

    fprintf(stdout, "\n\n DONE CONNECTING VOICE \n\n");

    char secret_key[1000];
    char ip[100];
    char port[100];
    char *ssrc = "66666";

    sm_get(pobj->vgt->data_dictionary, DISCORD_VOICE_IP, ip, 100);
    sm_get(pobj->vgt->data_dictionary, DISCORD_VOICE_PORT, port, 100);
    sm_get(pobj->vgt->data_dictionary, DISCORD_VOICE_SECRET_KEY, secret_key,
           1000);

    fprintf(stdout, "\n\nstarting media player.......\n\n");

    char filename[128] = {0};
    snprintf(filename, sizeof(filename) - 1, "audiotmpfile.%s.out",
             pobj->uobj.guild_id);

    fprintf(stdout, "setting up media player thread...\n");
    pobj->vgt->media =
        start_player(secret_key, ssrc, ip, port, pobj->vgt->voice_udp_sockfd,
                     filename, pobj->vgt);
  }

  write(STDOUT_FILENO, "\n", 1);
  write(STDOUT_FILENO, pobj->content, strlen(pobj->content));
  write(STDOUT_FILENO, "\n", 1);
  pobj->content += 3;


  fprintf(stdout, "Queueing song...\n");
  char title[200] = { 0 };
  int insert_queue_ret_error = 0;
  if (!strncasecmp(pobj->content, "https://", 8) && 1) {
    insert_queue_ret_error = insert_queue_ydl_query(pobj->vgt->media, pobj->content, title, sizeof(title));
    // If a playlist is found
    if (strstr(pobj->content, "&list=") != NULL)
    {
      FILE *fp;
      char cmd[1035] = "python3 py_scripts/youtube_parser.py playlist '";
      strcat(cmd, pobj->content);
      strcat(cmd, "'");
      fp = popen(cmd, "r");
      if (fp != NULL) {
        char buf[60000], ch;
        int i = 0;
        while ((ch = fgetc(fp)) != EOF) {
          buf[i++] = ch;
        }
        buf[i] = '\0';

        cJSON* video_json_list = cJSON_Parse(buf);
        int size = cJSON_GetArraySize(video_json_list);

        char msg[1024];
        sprintf(msg, "Queued %d songs", size);
        simple_send_msg(pobj->dis, msg, pobj->textchannelid);

        for (int i = 1; i < size; i++) {
          insert_queue_ytb_partial(pobj->vgt->media, cJSON_GetArrayItem(video_json_list, i));
        }

        cJSON_Delete(video_json_list);
      }
      pclose(fp);
    }
  } else {
    char youtube_dl_search_txt[2048];
    snprintf(youtube_dl_search_txt, 2048, "ytsearch1:%s", pobj->content);
    insert_queue_ret_error = insert_queue_ydl_query(pobj->vgt->media, youtube_dl_search_txt, title, sizeof(title));
  }

  if(!insert_queue_ret_error){
    char message[300];
    snprintf(message, sizeof(message), "Queued song: %s", title);
    simple_send_msg(pobj->dis, message ,pobj->textchannelid);
  }else{
    simple_send_msg(pobj->dis, "Unable to queue song. Please double check the link or whether video is age restricted." ,pobj->textchannelid);
  }

CLEANUP:

  sem_post(&(play_cmd_mutex));

  free(og_content);
  free(pobj->textchannelid);
  free(pobj);

  return NULL;
}

/* This function handles collecting titles from the queue
 *
 */
void get_queue_callback(void *value, int len, void *state, int pos, int start,
                        int end) {
  char **array = state;
  youtube_page_object_t *ytobj = value;

  array[pos - start] = malloc(len);

  if (ytobj->title[0] == 0) {
    get_youtube_vid_info(ytobj->link, ytobj);
  }

  char text3[sizeof(ytobj->title)];
  char text4[sizeof(ytobj->title)];

  escape_http_newline(ytobj->title, sizeof(ytobj->title), text3,
                      sizeof(ytobj->title));
  escape_http_doublequote(text3, sizeof(text3), text4, sizeof(text4));
  fix_string_ending(text4);

  memcpy(array[pos - start], text4, len);
}

/* set the prefix for each guild based on settings described in welcome channel
 * topic.
 *
 * bot needs to know what the prefix is...
 *
 */
void set_guild_config(discord_t *discord, char *msg, int msg_len) {
  int has_djroles_config = 0;

  cJSON *cjs = cJSON_ParseWithLength(msg, msg_len);
  cJSON *d_cjs = cJSON_GetObjectItem(cjs, "d");
  cJSON *gid_cjs = cJSON_GetObjectItem(d_cjs, "id");

  if (gid_cjs == NULL) {
    fprintf(stdout, "\nNULL obj\n");
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != NULL) {
      fprintf(stderr, "Error before: %s\n", error_ptr);
    }
  }
  fprintf(stdout, "\ngid:%s\n", gid_cjs->valuestring);

  cJSON *voice_states = cJSON_GetObjectItem(d_cjs, "voice_states");
  voice_states = voice_states->child;

  while (voice_states) {
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
  fprintf(stdout, "done parsing voice states...\n");

  char key[1000];
  cJSON *roles = cJSON_GetObjectItem(d_cjs, "roles");
  roles = roles->child;
  while (roles) {
    cJSON *name, *id;
    name = cJSON_GetObjectItem(roles, "name");
    id = cJSON_GetObjectItem(roles, "id");

    fprintf(stdout, "found role: %s:%s\n", name->valuestring, id->valuestring);

    snprintf(key, sizeof(key), "%s%s%s", DISCORD_GATEWAY_ROLE_MAP_ITEM, gid_cjs->valuestring, name->valuestring);

    sm_put(discord->data_dictionary, key, id->valuestring, strlen(id->valuestring) + 1);

    roles = roles->next;
  }

  char botnamesearch[100];
  snprintf(botnamesearch, sizeof(botnamesearch), "@%s", botname);

  char *settings = strstr(msg, botnamesearch);
  if (!settings) {
    goto CLEANUP_GUILD_CONFIG;
  }

  settings = strchr(settings, ' ');
  if (!settings) {
    goto CLEANUP_GUILD_CONFIG;
  }
  settings++;

  char roleid[100];
  while (strncasecmp(settings, "end", 3)) {
    if (!strncasecmp(settings, "prefix", 6)) {
      char prefix = *(settings + 6);

      snprintf(key, sizeof(key), "%s%s", DISCORD_GATEWAY_GUILD_PREFIX_SETTING,
               gid_cjs->valuestring);

      sm_put(discord->data_dictionary, key, &prefix, sizeof(prefix));
    } else if (!strncasecmp(settings, "djroles", 7)) {
      has_djroles_config = 1;

      char *role = settings + 8;
      char *end;
      while(role[0] != ' '){
        role++;
        end = strstr(role, "\\\"");
        *end = 0;
            
        snprintf(key, sizeof(key), "%s%s%s", DISCORD_GATEWAY_ROLE_MAP_ITEM, gid_cjs->valuestring, role);

        int found = sm_get(discord->data_dictionary, key, roleid, sizeof(roleid));
        
        if(found){
          snprintf(key, sizeof(key), "%s%s%s", DISCORD_GATEWAY_DJ_ROLES, gid_cjs->valuestring, roleid);
          fprintf(stdout, "djrole key: %s   ;%s\n", key, role);

          unsigned char allones = 0xFF;
          sm_put(discord->data_dictionary, key, (char *)&allones, sizeof(allones));
        }

        *end = '\\';
        role = end + 2;
        if(role[0] == ' ') 
          break;
        role += 2;
      }
    }


    settings = strchr(settings, ' ');
    if (!settings) {
      break;
    }
    settings++;
  }

  CLEANUP_GUILD_CONFIG:

  if(!has_djroles_config){
    fprintf(stdout, "No DJ roles found, allowing everyone permission\n");

    snprintf(key, sizeof(key), "%s%s%s", DISCORD_GATEWAY_DJ_ROLES, gid_cjs->valuestring, DISCORD_GATEWAY_ROLE_EVERYONE);
    unsigned char allones = 0xFF;
    sm_put(discord->data_dictionary, key, (char *)&allones, sizeof(allones));
  }

  cJSON_Delete(cjs);
}

void leave_command(voice_gateway_t *vgt, discord_t *dis, user_vc_obj *uobjp,
                   char *guildid, char *textchannelid, int wrong_vc,
                   int has_user, int is_dj) {
  if (!(has_user && (uobjp->vc_id[0] != 0) &&
        !strcmp(uobjp->guild_id, guildid))) {
    simple_send_msg(dis, "You must be in a voice channel!", textchannelid);
    return;
  }

  if (!(vgt && vgt->media && vgt->media->initialized && !wrong_vc)) {
    if (wrong_vc) {
      simple_send_msg(
          dis, "Please make sure I am joined or in the correct voice channel.",
          textchannelid);
    } else {
      simple_send_msg(dis, "No song playing!", textchannelid);
    }
    return;
  }

  if(!is_dj){
    simple_send_msg(dis, "You do not have permission to use this command!", textchannelid);
    return;
  }

  sem_wait(&(vgt->media->insert_song_mutex));

  fprintf(stdout, "\nLEAVING leaving... LEAVING\n");

  youtube_page_object_t yobj = {0};
  sbuf_insert_front_value((&(vgt->media->song_queue)), &yobj, sizeof(yobj));

  sem_post(&(vgt->media->quitter));
  sem_post(&(vgt->media->skipper));

  fprintf(stdout, "\nLEAVING leaving... LEAVING\n");
  char *ptr = 0;
  sm_put(dis->voice_gateway_map, uobjp->guild_id, (char *)&ptr, sizeof(void *));
  send_websocket(vgt->voice_ssl, "request close", strlen("request close"), 8);

  char msg[2000];
  snprintf(msg, 2000, DISCORD_GATEWAY_VOICE_LEAVE, uobjp->guild_id);
  sem_wait(&(dis->gateway_writer_mutex));
  send_websocket(dis->gateway_ssl, msg, strlen(msg), WEBSOCKET_OPCODE_MSG);
  sem_post(&(dis->gateway_writer_mutex));

  fprintf(stdout, "\nLEAVING leaving... LEAVING\n");
  free_voice_gateway(vgt);
  fprintf(stdout, "\nLEAVING leaving... LEAVING\n");
}

void skip_command(voice_gateway_t *vgt, discord_t *dis, user_vc_obj *uobjp,
                  char *guildid, char *textchannelid, int wrong_vc,
                  int has_user, int is_dj) {
  if (!(has_user && (uobjp->vc_id[0] != 0) &&
        !strcmp(uobjp->guild_id, guildid))) {
    simple_send_msg(dis, "You must be in a voice channel!", textchannelid);
    return;
  }

  if (!(vgt && vgt->media && vgt->media->playing && !wrong_vc)) {
    if (wrong_vc) {
      simple_send_msg(
          dis, "Please make sure I am joined or in the correct voice channel.",
          textchannelid);
    } else {
      simple_send_msg(dis, "No song playing!", textchannelid);
    }
    return;
  }

  if(!is_dj){
    simple_send_msg(dis, "You do not have permission to use this command!", textchannelid);
    return;
  }

  sem_post(&(vgt->media->skipper));
}

void desc_command(voice_gateway_t *vgt, discord_t *dis, user_vc_obj *uobjp,
                  char *guildid, char *textchannelid, int wrong_vc,
                  int has_user) {
  if (!(has_user && (uobjp->vc_id[0] != 0) &&
        !strcmp(uobjp->guild_id, guildid))) {
    simple_send_msg(dis, "You must be in a voice channel!", textchannelid);
    return;
  }

  if (!(vgt && vgt->media && vgt->media->playing && !wrong_vc)) {
    if (wrong_vc) {
      simple_send_msg(
          dis, "Please make sure I am joined or in the correct voice channel.",
          textchannelid);
    } else {
      simple_send_msg(dis, "No song playing!", textchannelid);
    }
    return;
  }

  fprintf(stdout, "\ntrying to send msg...\n");

  ssl_reconnect(dis->https_api_ssl, DISCORD_HOST, strlen(DISCORD_HOST),
                DISCORD_PORT, strlen(DISCORD_PORT));

  char message[9500];

  if (vgt->media && vgt->media->playing) {
    youtube_page_object_t ytpobj;
    sbuf_peek_end_value(&(vgt->media->song_queue), &(ytpobj), sizeof(ytpobj),
                        0);

    if (ytpobj.description[0] == 0) {
      get_youtube_vid_info(ytpobj.link, &ytpobj);
    }

    char text[sizeof(ytpobj.description)];
    char text2[sizeof(ytpobj.description)];

    escape_http_newline(ytpobj.description, sizeof(ytpobj.description), text,
                        sizeof(ytpobj.description));

    fprintf(stdout, "\n%s\n\n\n", text);
    fflush(stdout);

    escape_http_doublequote(text, sizeof(text), text2, sizeof(text2));

    fix_string_ending(text2);

    char text3[sizeof(ytpobj.title)];
    char text4[sizeof(ytpobj.title)];

    escape_http_newline(ytpobj.title, sizeof(ytpobj.title), text3,
                        sizeof(ytpobj.title));
    escape_http_doublequote(text3, sizeof(text3), text4, sizeof(text4));
    fix_string_ending(text4);

    snprintf(message, 9500, DISCORD_API_POST_BODY_MSG_EMBED,
             "Now Playing:", text4, ytpobj.link, text2);
    // snprintf(message, 9500, DISCORD_API_POST_BODY_MSG_SIMPLE, "Not currently
    // playing a song!");
  } else {
    snprintf(message, 9500, DISCORD_API_POST_BODY_MSG_SIMPLE,
             "Not currently playing a song!");
  }

  char header[2000];
  snprintf(header, 2000, DISCORD_API_POST_MSG, textchannelid, bottoken,
           (int)strlen(message));
  char buffer[13000];
  snprintf(buffer, 13000, "%s\r\n\r\n%s\r\n\r\n", header, message);
  fprintf(stdout, buffer);
  send_raw(dis->https_api_ssl, buffer, strlen(buffer));
}

void now_playing_command(voice_gateway_t *vgt, discord_t *dis,
                         user_vc_obj *uobjp, char *guildid, char *textchannelid,
                         int wrong_vc, int has_user) {
  if (!(has_user && (uobjp->vc_id[0] != 0) &&
        !strcmp(uobjp->guild_id, guildid))) {
    simple_send_msg(dis, "You must be in a voice channel!", textchannelid);
    return;
  }

  if (!(vgt && vgt->media && vgt->media->playing && !wrong_vc)) {
    if (wrong_vc) {
      simple_send_msg(
          dis, "Please make sure I am joined or in the correct voice channel.",
          textchannelid);
    } else {
      simple_send_msg(dis, "No song playing!", textchannelid);
    }
    return;
  }

  fprintf(stdout, "\ntrying to send msg...\n");
  ssl_reconnect(dis->https_api_ssl, DISCORD_HOST, strlen(DISCORD_HOST),
                DISCORD_PORT, strlen(DISCORD_PORT));
  char message[9500];

  if (vgt->media && vgt->media->playing) {
    youtube_page_object_t ytpobj;
    sbuf_peek_end_value(&(vgt->media->song_queue), &(ytpobj), sizeof(ytpobj),
                        0);

    char text3[sizeof(ytpobj.title)];
    char text4[sizeof(ytpobj.title)];

    escape_http_newline(ytpobj.title, sizeof(ytpobj.title), text3,
                        sizeof(ytpobj.title));
    escape_http_doublequote(text3, sizeof(text3), text4, sizeof(text4));
    fix_string_ending(text4);

    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    long lapse = now.tv_sec - vgt->media->song_start_time.tv_sec;

    fprintf(stdout, "lapse: %ld\n", lapse);

#define barsize 40
    char bar[barsize] = {0};
    long progress = (barsize - 2) * lapse / (ytpobj.length_in_seconds);
    for (int i = 0; i < barsize - 2; i++) {
      if (i < progress)
        strcat(bar, "#");
      else
        strcat(bar, "-");
    }

    fprintf(stdout, "bar: %s\n", bar);

    char time_str[200] = {0};
    char hour[10] = {0};
    if (ytpobj.length_in_seconds / 3600 > 0) {
      sprintf(hour, "%02ld:", lapse / 3600);
    }

    snprintf(time_str, sizeof(time_str), "%s%02ld:%02ld [%s] %s", hour,
             (lapse / 60) % 60, lapse % 60, bar, ytpobj.duration);

    snprintf(message, 9500, DISCORD_API_POST_BODY_MSG_EMBED,
             "Now Playing:", text4, ytpobj.link, time_str);
  } else {
    snprintf(message, 9500, DISCORD_API_POST_BODY_MSG_SIMPLE,
             "Not currently playing a song!");
  }
  char header[2000];
  snprintf(header, 2000, DISCORD_API_POST_MSG, textchannelid, bottoken,
           (int)strlen(message));
  char buffer[13000];
  snprintf(buffer, 13000, "%s\r\n\r\n%s\r\n\r\n", header, message);
  fprintf(stdout, buffer);
  send_raw(dis->https_api_ssl, buffer, strlen(buffer));
}

void show_queue_command(voice_gateway_t *vgt, discord_t *dis,
                        user_vc_obj *uobjp, char *guildid, char *textchannelid,
                        int wrong_vc, int has_user) {
  if (!(has_user && (uobjp->vc_id[0] != 0) &&
        !strcmp(uobjp->guild_id, guildid))) {
    simple_send_msg(dis, "You must be in a voice channel!", textchannelid);
    return;
  }

  if (!(vgt && vgt->media && vgt->media->playing && !wrong_vc)) {
    if (wrong_vc) {
      simple_send_msg(
          dis, "Please make sure I am joined or in the correct voice channel.",
          textchannelid);
    } else {
      simple_send_msg(dis, "No song playing!", textchannelid);
    }
    return;
  }

  fprintf(stdout, "\ntrying to send msg...\n");
  ssl_reconnect(dis->https_api_ssl, DISCORD_HOST, strlen(DISCORD_HOST),
                DISCORD_PORT, strlen(DISCORD_PORT));
  char message[9500];

  if (vgt->media && vgt->media->playing) {
    char *(title_arr[5]) = {0};
    sbuf_iterate(&(vgt->media->song_queue), get_queue_callback, title_arr, 0,
                 4);

    char inner_message[5000];
    snprintf(inner_message, 5000, "1. %s\\n2. %s\\n3. %s\\n4. %s\\n5. %s\\n",
             title_arr[0], title_arr[1], title_arr[2], title_arr[3],
             title_arr[4]);

    for (int i = 0; i < 5; i++) {
      free(title_arr[i]);
    }

    snprintf(message, 9500, DISCORD_API_POST_BODY_MSG_EMBED,
             "Song Queue:", "Up next on the playlist...", inner_message,
             "To see more songs, use \\\"queue [page number]\\\"");
  } else {
    snprintf(message, 9500, DISCORD_API_POST_BODY_MSG_SIMPLE,
             "No song playing!");
  }
  char header[2000];
  snprintf(header, 2000, DISCORD_API_POST_MSG, textchannelid, bottoken,
           (int)strlen(message));
  char buffer[13000];
  snprintf(buffer, 13000, "%s\r\n\r\n%s\r\n\r\n", header, message);
  fprintf(stdout, buffer);
  send_raw(dis->https_api_ssl, buffer, strlen(buffer));
}

void play_command(voice_gateway_t *vgt, discord_t *dis, user_vc_obj *uobjp,
                  char *guildid, char *textchannelid, char *content,
                  int wrong_vc, int has_user, int is_dj) {
  if (!(has_user && (uobjp->vc_id[0] != 0) &&
        !strcmp(uobjp->guild_id, guildid))) {
    simple_send_msg(dis, "You must be in a voice channel!", textchannelid);
    return;
  }

  if (wrong_vc) {
    simple_send_msg(
        dis, "Please make sure I am joined or in the correct voice channel.",
        textchannelid);
    return;
  }

  if(!is_dj){
    simple_send_msg(dis, "You do not have permission to use this command!", textchannelid);
    return;
  }

  struct play_cmd_obj *pobj = malloc(sizeof(struct play_cmd_obj));
  pobj->dis = dis;
  pobj->vgt = vgt;
  pobj->uobj = *uobjp;

  pobj->content = malloc(strlen(content) + 1);
  strcpy(pobj->content, content);
  pobj->textchannelid = malloc(strlen(textchannelid) + 1);
  strcpy(pobj->textchannelid, textchannelid);

  pthread_t tid;
  pthread_create(&tid, NULL, threaded_play_cmd, pobj);
}

int check_user_dj_role(discord_t *dis, cJSON *cjs, char *guildid){
  char key[200];
  snprintf(key, sizeof(key), "%s%s%s", DISCORD_GATEWAY_DJ_ROLES, guildid, DISCORD_GATEWAY_ROLE_EVERYONE);
  int found = sm_get(dis->data_dictionary, key, NULL, 0);
  if(found){
    return 1;
  }

  cJSON *d_cjs = cJSON_GetObjectItem(cjs, "d");
  cJSON *member_cjs = cJSON_GetObjectItem(d_cjs, "member");
  cJSON *roles = cJSON_GetObjectItem(member_cjs, "roles");

  cJSON *role = roles->child;
  while(role){
    snprintf(key, sizeof(key), "%s%s%s", DISCORD_GATEWAY_DJ_ROLES, guildid, role->valuestring);
    found = sm_get(dis->data_dictionary, key, NULL, 0);
    if(found){
      return 1;
    }

    role = role->next;
  }

  fprintf(stdout, "USER NOT DJ.\n");
  return 0;
}

/* Gateway callback. handle all messages
 *
 * messages should be checked for proper tags...
 *
 */
void actually_do_shit(void *state, char *msg, unsigned long msg_len) {
  discord_t *dis = state;

  // debug spit out
  //write(STDOUT_FILENO, msg, msg_len);
  //write(STDOUT_FILENO, "WOW WOW WOW\n", strlen("WOW WOW WOW\n"));

  // handle adding members on startup
  if (strcasestr(msg, "\"GUILD_CREATE\"")) {
    set_guild_config(dis, msg, msg_len);
    return;
  }

  // debug disconnect gateway
  if (strstr(msg, "HelloFromTheOtherSide1234")) {
    send_websocket(dis->gateway_ssl, "request \"\"\" close",
                   strlen("request close"), 8);
    return;
  }

  // if someone sends a message
  if (strcasestr(msg, "\"MESSAGE_CREATE\"")) {
    char *guildid_cp = 0;
    char *userid_cp = 0;
    char *textchannelid_nw = 0;

    char *content = strcasestr(msg, "content") + 10;

    // get guild_id
    char *guildid = strcasestr(msg, "guild_id\"");
    guildid += 11;
    char *end = strchr(guildid, '"');
    *end = 0;
    guildid_cp = malloc(strlen(guildid) + 1);
    strcpy(guildid_cp, guildid);
    *end = '"';
    guildid = guildid_cp;
    fprintf(stdout, "\n\nDETECTED guild id: %s\n\n", guildid);

    // get the bot prefix for this guild
    char botprefix[10] = {0};
    botprefix[0] = BOT_PREFIX[0];
    char key[200];
    snprintf(key, sizeof(key), "%s%s", DISCORD_GATEWAY_GUILD_PREFIX_SETTING,
             guildid);
    sm_get(dis->data_dictionary, key, botprefix, sizeof(botprefix));

    // handle bot prefix changing
    if ((content[0] == botprefix[0]) &&
        !strncasecmp(content + 1, "prefix", 6)) {
      botprefix[0] = *(content + 8);
      sm_put(dis->data_dictionary, key, &botprefix[0], sizeof(botprefix[0]));
      goto CLEANUP_CREATE_MESSAGE;
    }

    // get the user id of the person sending message
    char *userid = strcasestr(msg, DISCORD_GATEWAY_VOICE_USERNAME);
    if (userid) {
      userid = strcasestr(userid, DISCORD_GATEWAY_VOICE_USER_ID);
      userid += 6;
      end = strchr(userid, '"');
      *end = 0;
      userid_cp = malloc(strlen(userid) + 1);
      strcpy(userid_cp, userid);
      *end = '"';
      userid = userid_cp;
      fprintf(stdout, "\n\nDETECTED USER: %s\n\n", userid);
    }

    // get channel id of the text channel where the message was sent
    char *textchannelid = strcasestr(msg, DISCORD_GATEWAY_MSG_CHANNEL_ID);
    if (textchannelid) {
      textchannelid += 14;
      end = strchr(textchannelid, '"');
      *end = 0;

      textchannelid_nw = malloc(strlen(textchannelid) + 1);
      strcpy(textchannelid_nw, textchannelid);

      *end = '"';
      textchannelid = textchannelid_nw;

      fprintf(stdout, "\n\nTEXT CHANNEL: %s\n\n", textchannelid);
    }

    cJSON *cjs = cJSON_ParseWithLength(msg, msg_len);
    int is_dj = check_user_dj_role(dis, cjs, guildid);
    cJSON_Delete(cjs);


    // collect user info from USER INFO MAP
    user_vc_obj uobj;
    int has_user =
        sm_get(dis->user_vc_map, userid, (char *)&uobj, sizeof(uobj));
    fprintf(stdout, "\n\nUSER IN CHANNEL: %s, guild: %s\n\n", uobj.vc_id,
            uobj.guild_id);

    // cut the content string for easier processing
    end = strstr(content, "\",\"");
    *end = 0;

    // collect voice gateway from the gateway map
    voice_gateway_t *vgt = 0;
    sm_get(dis->voice_gateway_map, uobj.guild_id, (char *)&vgt, sizeof(void *));

    // check if user is in the wrong voice channel
    char bot_channel_id[100] = {0};
    int wrong_vc = 0;
    if (vgt) {
      int found =
          sm_get(vgt->data_dictionary, DISCORD_VOICE_STATE_UPDATE_CHANNEL_ID,
                 bot_channel_id, sizeof(bot_channel_id));
      wrong_vc = found && strcmp(bot_channel_id, uobj.vc_id);
    }

    if ((content[0] == botprefix[0])) {
      if (!strncasecmp(content + 1, "leave", 5)) {
        leave_command(vgt, dis, &uobj, guildid, textchannelid, wrong_vc,
                      has_user, is_dj);
      } else if (!strncasecmp(content + 1, "skip", 4)) {
        skip_command(vgt, dis, &uobj, guildid, textchannelid, wrong_vc,
                     has_user, is_dj);
      } else if (!strncasecmp(content + 1, "desc", 4)) {
        desc_command(vgt, dis, &uobj, guildid, textchannelid, wrong_vc,
                     has_user);
      } else if (!strncasecmp(content + 1, "np", 2)) {
        now_playing_command(vgt, dis, &uobj, guildid, textchannelid, wrong_vc,
                            has_user);
      } else if (!strncasecmp(content + 1, "queue", 5)) {
        show_queue_command(vgt, dis, &uobj, guildid, textchannelid, wrong_vc,
                           has_user);
      } else if (!strncasecmp(content + 1, "p ", 2)) {
        play_command(vgt, dis, &uobj, guildid, textchannelid, content, wrong_vc,
                     has_user, is_dj);
      }
    }

    CLEANUP_CREATE_MESSAGE:

    if (userid_cp) {
      free(userid_cp);
    }
    if (guildid_cp) {
      free(guildid_cp);
    }
    if (textchannelid_nw) {
      free(textchannelid_nw);
    }
  }
}

int main(int argc, char **argv) {
  sem_init(&(play_cmd_mutex), 0, 1);

  botname = getenv("BOT_NAME");
  if (!botname) {
    botname = DEFAULT_BOT_NAME;
  }

  if (argc > 1) {
    bottoken = argv[1];
  } else {
    bottoken = getenv("TOKEN");
  }
  discord_t *discord = init_discord(bottoken, "641");

  fprintf(stdout, "Token: %s\n", BOT_PREFIX);

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
