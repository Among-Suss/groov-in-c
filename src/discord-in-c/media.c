/* Copyright 2012 Mozilla Foundation
   Copyright 2012 Xiph.Org Foundation
   Copyright 2012 Gregory Maxwell

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "sbuf.structs.h"
#include "media.h"
#include "discord.structs.h"
#include "media.structs.h"
#include "litesocket/litesocket.structs.h"
#include "utils.h"
#include "youtube_fetch.h"

// helpers from opusrtp.c

// defines from opusrtp.c
#define RTP_HEADER_MIN 12
typedef struct rtp_header {
  int version;
  int type;
  int pad, ext, cc, mark;
  int seq, time;
  int ssrc;
  int *csrc;
  int header_size;
  int payload_size;
} rtp_header;

/* helper, write a little-endian 32 bit int to memory */
void le32(unsigned char *p, int v) {
  p[0] = v & 0xff;
  p[1] = (v >> 8) & 0xff;
  p[2] = (v >> 16) & 0xff;
  p[3] = (v >> 24) & 0xff;
}

/* helper, write a little-endian 16 bit int to memory */
void le16(unsigned char *p, int v) {
  p[0] = v & 0xff;
  p[1] = (v >> 8) & 0xff;
}

/* helper, write a big-endian 32 bit int to memory */
void be32(unsigned char *p, int v) {
  p[0] = (v >> 24) & 0xff;
  p[1] = (v >> 16) & 0xff;
  p[2] = (v >> 8) & 0xff;
  p[3] = v & 0xff;
}

/* helper, write a big-endian 16 bit int to memory */
void be16(unsigned char *p, int v) {
  p[0] = (v >> 8) & 0xff;
  p[1] = v & 0xff;
}

/* check if an ogg page begins an opus stream */
int is_opus(ogg_page *og) {
  ogg_stream_state os;
  ogg_packet op;

  ogg_stream_init(&os, ogg_page_serialno(og));
  ogg_stream_pagein(&os, og);
  if (ogg_stream_packetout(&os, &op) == 1) {
    if (op.bytes >= 19 && !memcmp(op.packet, "OpusHead", 8)) {
      ogg_stream_clear(&os);
      return 1;
    }
  }
  ogg_stream_clear(&os);
  return 0;
}

int serialize_rtp_header(unsigned char *packet, int size, rtp_header *rtp) {
  int i;

  if (!packet || !rtp) {
    return -2;
  }
  if (size < RTP_HEADER_MIN) {
    fprintf(stderr, "Packet buffer too short for RTP\n");
    return -1;
  }
  if (size < rtp->header_size) {
    fprintf(stderr, "Packet buffer too short for declared RTP header size\n");
    return -3;
  }
  packet[0] = ((rtp->version & 3) << 6) | ((rtp->pad & 1) << 5) |
              ((rtp->ext & 1) << 4) | ((rtp->cc & 7));
  packet[1] = ((rtp->mark & 1) << 7) | ((rtp->type & 127));
  be16(packet + 2, rtp->seq);
  be32(packet + 4, rtp->time);
  be32(packet + 8, rtp->ssrc);
  if (rtp->cc && rtp->csrc) {
    for (i = 0; i < rtp->cc; i++) {
      be32(packet + 12 + i * 4, rtp->csrc[i]);
    }
  }

  return 0;
}

int update_rtp_header(rtp_header *rtp) {
  rtp->header_size = 12 + 4 * rtp->cc;
  return 0;
}

// end....

// TIME SLOT WAITER FUNCTIONS

typedef struct {
#if defined HAVE_MACH_ABSOLUTE_TIME
  /* Apple */
  mach_timebase_info_data_t tbinfo;
  uint64_t target;
#elif defined HAVE_CLOCK_GETTIME && defined CLOCK_REALTIME &&                  \
    defined HAVE_NANOSLEEP
  /* try to use POSIX monotonic clock */
  int initialized;
  clockid_t clock_id;
  struct timespec target;
#else
  /* fall back to the old non-monotonic gettimeofday() */
  int initialized;
  struct timeval target;
#endif
} time_slot_wait_t;

void init_time_slot_wait(time_slot_wait_t *pt) {
  memset(pt, 0, sizeof(time_slot_wait_t));
}

/*
 * Wait for the next time slot, which begins delta nanoseconds after the
 * start of the previous time slot, or in the case of the first call at
 * the time of the call.  delta must be in the range 0..999999999.
 */
void wait_for_time_slot(int delta, time_slot_wait_t *state) {
#if defined HAVE_MACH_ABSOLUTE_TIME
  /* Apple */

  if (state->tbinfo.numer == 0) {
    mach_timebase_info(&(state->tbinfo));
    state->target = mach_absolute_time();
  } else {
    state->target +=
        state->tbinfo.numer == state->tbinfo.denom
            ? (uint64_t)delta
            : (uint64_t)delta * state->tbinfo.denom / state->tbinfo.numer;
    mach_wait_until(state->target);
  }
#elif defined HAVE_CLOCK_GETTIME && defined CLOCK_REALTIME &&                  \
    defined HAVE_NANOSLEEP
  /* try to use POSIX monotonic clock */

  if (!state->initialized) {
#if defined CLOCK_MONOTONIC && defined _POSIX_MONOTONIC_CLOCK &&               \
    _POSIX_MONOTONIC_CLOCK >= 0
    if (
#if _POSIX_MONOTONIC_CLOCK == 0
        sysconf(_SC_MONOTONIC_CLOCK) > 0 &&
#endif
        clock_gettime(CLOCK_MONOTONIC, &state->target) == 0) {
      state->clock_id = CLOCK_MONOTONIC;
      state->initialized = 1;
    } else
#endif
        if (clock_gettime(CLOCK_REALTIME, &state->target) == 0) {
      state->clock_id = CLOCK_REALTIME;
      state->initialized = 1;
    }
  } else {
    state->target.tv_nsec += delta;
    if (state->target.tv_nsec >= 1000000000) {
      ++state->target.tv_sec;
      state->target.tv_nsec -= 1000000000;
    }
#if defined HAVE_CLOCK_NANOSLEEP && defined _POSIX_CLOCK_SELECTION &&          \
    _POSIX_CLOCK_SELECTION > 0
    clock_nanosleep(state->clock_id, TIMER_ABSTIME, &state->target, NULL);
#else
    {
      /* convert to relative time */
      struct timespec rel;
      if (clock_gettime(clock_id, &rel) == 0) {
        rel.tv_sec = state->target.tv_sec - rel.tv_sec;
        rel.tv_nsec = state->target.tv_nsec - rel.tv_nsec;
        if (rel.tv_nsec < 0) {
          rel.tv_nsec += 1000000000;
          --rel.tv_sec;
        }
        if (rel.tv_sec >= 0 && (rel.tv_sec > 0 || rel.tv_nsec > 0)) {
          nanosleep(&rel, NULL);
        }
      }
    }
#endif
  }
#else
  /* fall back to the old non-monotonic gettimeofday() */
  struct timeval now;
  int nap;

  if (!state->initialized) {
    gettimeofday(&state->target, NULL);
    state->initialized = 1;
  } else {
    delta /= 1000;
    state->target.tv_usec += delta;
    if (state->target.tv_usec >= 1000000) {
      ++state->target.tv_sec;
      state->target.tv_usec -= 1000000;
    }

    gettimeofday(&now, NULL);
    nap = state->target.tv_usec - now.tv_usec;
    if (now.tv_sec != state->target.tv_sec) {
      if (now.tv_sec > state->target.tv_sec)
        nap = 0;
      else if (state->target.tv_sec - now.tv_sec == 1)
        nap += 1000000;
      else
        nap = 1000000;
    }
    if (nap > delta)
      nap = delta;
    if (nap > 0) {
#if defined HAVE_USLEEP
      usleep(nap);
#else
      struct timeval timeout;
      timeout.tv_sec = 0;
      timeout.tv_usec = nap;
      select(0, NULL, NULL, NULL, &timeout);
#endif
    }
  }
#endif
}

int send_rtp_packet(int fd, struct sockaddr *addr, socklen_t addrlen,
                    rtp_header *rtp, const unsigned char *opus_packet,
                    unsigned char *key) {
  int ret;
  unsigned char packet[65535];

  update_rtp_header(rtp);
  serialize_rtp_header(packet, rtp->header_size, rtp);

  // ENCRYPT HERE ___ENCRYPT OPUS PACKET___opus_packet
  unsigned char nonce[24];
  memcpy(nonce, packet, 12);
  memset(nonce + 12, 0, 12);

  crypto_secretbox_easy(packet + rtp->header_size, opus_packet,
                        rtp->payload_size, nonce, key);

  ret = sendto(fd, packet,
               rtp->header_size + rtp->payload_size + crypto_secretbox_MACBYTES,
               0, addr, addrlen);

  if (ret < 0) {
    fprintf(stderr, "error sending: %s\n", strerror(errno));
  }

  return ret;
}

int rtp_send_file_to_addr(const char *filename, int payload_type, int ssrc,
                          int *ffmpeg_running_state,
                          media_player_t *media_obj_ptr) {
  /* POSIX MONOTONIC CLOCK MEASURER */
  clockid_t clock_id;
  struct timespec start, end;
  sysconf(_SC_MONOTONIC_CLOCK);
  clock_id = CLOCK_MONOTONIC;
  clock_gettime(clock_id, &start);

  time_slot_wait_t state;
  init_time_slot_wait(&state);

  rtp_header rtp;
  int ret;
  int in_fd;
  ogg_sync_state oy;
  ogg_stream_state os;
  ogg_page og;
  ogg_packet op;
  int headers = 0, read_test_len;
  char *in_data, dummy_char;
  const long in_size = 8192; //= 8192;
  size_t in_read;

  rtp.version = 2;
  rtp.type = payload_type;
  rtp.pad = 0;
  rtp.ext = 0;
  rtp.cc = 0;
  rtp.mark = 0;
  rtp.seq = rand();
  rtp.time = rand();
  rtp.ssrc = ssrc;
  rtp.csrc = NULL;
  rtp.header_size = 0;
  rtp.payload_size = 0;

  fprintf(stderr, "Starting sender\n\nSending %s\n\n", filename);
  in_fd = open(filename, O_RDONLY, 0644);
  if (!in_fd) {
    fprintf(stderr, "ERROR: cannot open file in sender\n");
  }

  ret = ogg_sync_init(&oy);
  if (ret < 0) {
    fprintf(stdout, "OGG ERROR\n");
  }

  while ((read_test_len = read(in_fd, &dummy_char, 1)) ||
         *ffmpeg_running_state) {

    if (media_obj_ptr && sem_trywait(&(media_obj_ptr->skipper)) >= 0) {
      fprintf(stdout,
              "Media Player received skip signal, stopping playback.\n");
      break;
    }

    if (read_test_len == 1) {
      lseek(in_fd, -1, SEEK_CUR);
    } else {
      wait_for_time_slot(20000000, &state);
      continue;
    }

    in_data = ogg_sync_buffer(&oy, in_size);
    if (!in_data) {
      fprintf(stderr, "ERROR: ogg_sync_buffer\n");
    }

    in_read = read(in_fd, in_data, in_size);
    ret = ogg_sync_wrote(&oy, in_read);
    if (ret < 0) {
      fprintf(stderr, "ERROR: ogg_sync_wrote\n");
    }

    while (ogg_sync_pageout(&oy, &og) == 1) {
      if (headers == 0) {
        if (is_opus(&og)) {
          /* this is the start of an Opus stream */
          ret = ogg_stream_init(&os, ogg_page_serialno(&og));
          if (ret < 0) {
            fprintf(stderr, "ERROR: ogg_page_serialno\n");
          }
          headers++;
        } else if (!ogg_page_bos(&og)) {
          /* We're past the header and haven't found an Opus stream.
           * Time to give up. */
          close(in_fd);
          return 1;
        } else {
          /* try again */
          continue;
        }
      }
      /* submit the page for packetization */
      ret = ogg_stream_pagein(&os, &og);
      if (ret < 0) {
        fprintf(stderr, "ERROR: ogg_stream_pagein\n");
      }

      /* read and process available packets */
      while (ogg_stream_packetout(&os, &op) == 1) {
        int samples;
        /* skip header packets */
        if (headers == 1 && op.bytes >= 19 &&
            !memcmp(op.packet, "OpusHead", 8)) {
          headers++;
          continue;
        }
        if (headers == 2 && op.bytes >= 16 &&
            !memcmp(op.packet, "OpusTags", 8)) {
          headers++;
          continue;
        }
        /* get packet duration */
        samples = opus_packet_get_nb_samples(op.packet, op.bytes, 48000);
        if (samples <= 0) {
          fprintf(stderr, "skipping invalid packet\n");
          continue;
        }

        /* update the rtp header and send */
        media_obj_ptr->current_song_time =
            media_obj_ptr->current_song_time +
            (((double)samples) * 62500.0 / 3.0 / 1000000000.0);
        rtp.seq++;
        rtp.time += samples;
        rtp.payload_size = op.bytes;
        if (media_obj_ptr) {
          sem_wait(&(media_obj_ptr->destination_info_mutex));
          send_rtp_packet(media_obj_ptr->udp_fd, media_obj_ptr->addr_malloc,
                          media_obj_ptr->addrlen, &rtp, op.packet,
                          media_obj_ptr->key);
          sem_post(&(media_obj_ptr->destination_info_mutex));
        } else {
          fprintf(stderr, "ERROR: media player obj is NULL.\n");
        }

        // no need to log abnormal frame anymore
        /*

        // timer to verify that sender is on time
        clock_gettime(clock_id, &end);
        long secediff = (long int)end.tv_sec - start.tv_sec;
        long usecdiff =
            (long)((end.tv_nsec - start.tv_nsec) / 1000) + secediff * 1000000;


        if (usecdiff > 25000 || usecdiff < 15000) {
          log_trace("Audio Sender Abnormal Frame: "
                    "... "
                    "%ld\n",
                    usecdiff);
        }


        start = end;
        */

        /* convert number of 48 kHz samples to nanoseconds without overflow */
        wait_for_time_slot(samples * 62500 / 3, &state);

        // pause function
        int pauser_value;
        sem_getvalue(&(media_obj_ptr->pauser), &pauser_value);
        sem_wait(&(media_obj_ptr->pauser));
        sem_post(&(media_obj_ptr->pauser));
        if (pauser_value == 0) {
          fprintf(stdout, "Pause detected, reinitializing packet timer.\n");
          state.initialized = 0;
          wait_for_time_slot(0, &state);
        }
      }
    }
  }

  if (headers > 0)
    ogg_stream_clear(&os);
  ogg_sync_clear(&oy);
  close(in_fd);
  return 0;
}

int rtp_send_file(const char *filename, const char *dest, const char *port,
                  int payload_type, unsigned char *key, int ssrc,
                  int *ffmpeg_running_state, media_player_t *media_obj_ptr) {
  int ret;
  struct addrinfo *addrs;
  struct addrinfo hints;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = 0;
  hints.ai_protocol = IPPROTO_UDP;
  ret = getaddrinfo(dest, port, &hints, &addrs);
  if (ret != 0 || !addrs) {
    fprintf(stderr, "Cannot resolve host %s port %s: %s\n", dest, port,
            gai_strerror(ret));
    return -1;
  }

  memcpy(media_obj_ptr->addr_malloc, addrs->ai_addr, sizeof(struct sockaddr));
  media_obj_ptr->addrlen = addrs->ai_addrlen;
  memcpy(media_obj_ptr->key, key, sizeof(media_obj_ptr->key));

  freeaddrinfo(addrs);

  ret = rtp_send_file_to_addr(filename, payload_type, ssrc,
                              ffmpeg_running_state, media_obj_ptr);
  return ret;
}

void *ffmpeg_process_waiter(void *ptr) {
  pthread_detach(pthread_self());

  ffmpeg_process_waiter_t *fptr = (ffmpeg_process_waiter_t *)ptr;
  pid_t pid = fptr->pid;
  int *ffmpeg_process_state = fptr->ffmpeg_process_state;

  while (waitpid(pid, NULL, 0) < 0)
    ;

  *ffmpeg_process_state = 0;

  sem_wait(&(fptr->wait_until_song_finish));

  free(fptr->ffmpeg_process_state);
  free(fptr);
  return NULL;
}

void play_youtube_url(char *youtube_link, int time_offset, char *key_str,
                      char *ssrc_str, char *dest_address, char *dest_port,
                      const int sockfd, char *cache_file_unique_name,
                      media_player_t *media_obj_ptr) {
  char *url = youtube_link;

  int fd = open(cache_file_unique_name, O_CREAT | O_RDWR | O_TRUNC, 0644);
  close(fd);

  // converting start time to string
  char star_time_str[64] = {0};
  snprintf(star_time_str, sizeof(star_time_str), "%d", time_offset);

  fprintf(stdout, "Running ffmpeg...\n");

  char *log_level_env = getenv("LOG_LEVEL");

  pid_t pid;
  if ((pid = fork()) == 0) {
    char *new_argv[50] = {
        "ffmpeg",

        "-hide_banner",

        "-loglevel",
        (log_level_env ? atoi(log_level_env) : 0 > 0) ? "info" : "error",

        "-ss",
        star_time_str,

        "-i",
        url,

        "-c:a",
        "libopus",

        "-b:a",
        "64k",

        "-vbr",
        "off",

        "-compression_level",
        "10",

        "-frame_duration",
        "20",

        "-application",
        "audio",

        "-f",
        "opus",

        "-flush_packets",
        "1",

        "-y",
        cache_file_unique_name,
        0};

    if (execvp(new_argv[0], new_argv) < 0) {
      fprintf(stderr, "UNIX EXECVE ERROR\n");
      exit(1);
    }
  }

  int *ffmpeg_state_value_pointer = malloc(sizeof(int));
  *ffmpeg_state_value_pointer = 1;
  ffmpeg_process_waiter_t *fptr = malloc(sizeof(ffmpeg_process_waiter_t));

  fptr->pid = pid;
  fptr->ffmpeg_process_state = ffmpeg_state_value_pointer;
  sem_init(&(fptr->wait_until_song_finish), 0, 0);

  pthread_t tid;
  pthread_create(&tid, NULL, ffmpeg_process_waiter, fptr);

  unsigned char diskey[32];
  char *end;
  char *key_str_cpy = malloc(strlen(key_str) + 1);
  strcpy(key_str_cpy, key_str);
  key_str = key_str_cpy;

  int i = 0;
  while ((end = strchr(key_str, ','))) {
    *end = 0;
    diskey[i] = atoi(key_str);
    key_str = end + 1;
    i++;
  }
  diskey[31] = atoi(key_str);
  free(key_str_cpy);

  int ssrc = atoi(ssrc_str);

  fprintf(stdout, "Sending file using custom opus sender/encrypter...\n");

  rtp_send_file(cache_file_unique_name, dest_address, dest_port, 120, diskey,
                ssrc, ffmpeg_state_value_pointer, media_obj_ptr);

  kill(pid, SIGKILL);
  remove(cache_file_unique_name);

  sem_post(&(fptr->wait_until_song_finish));
}

void *media_player_threaded(void *ptr) {
  struct timespec now;
  pthread_detach(pthread_self());
  youtube_player_t *yptr = (youtube_player_t *)ptr;

  yptr->media_player_t_ptr->initialized = 1;

  youtube_page_object_t *ytpobj_p;
  char link[MAX_URL_LEN_MEDIA];
  int start_time_offset;
  while (sem_trywait(&(yptr->media_player_t_ptr->quitter)) < 0) {
    ytpobj_p = sbuf_peek_end_value_direct(
        &(yptr->media_player_t_ptr->song_queue), NULL, 1);

    if (!ytpobj_p) {
      sbuf_stop_peeking(&(yptr->media_player_t_ptr->song_queue));
      continue;
    }

    if (sem_trywait(&(yptr->media_player_t_ptr->quitter)) >= 0) {
      break;
    }

    // If partial object from playlist
    if (ytpobj_p->audio_url[0] == 0) {
      complete_youtube_object_fields(ytpobj_p);
    }
    // check if link is too old.
    clock_gettime(CLOCK_REALTIME, &now);
    long video_age = now.tv_sec - ytpobj_p->audio_url_create_date.tv_sec;
    fprintf(stdout, "Video age:%ld\n", video_age);
    if (!(video_age < 600 && video_age >= 0)) {
      complete_youtube_object_fields(ytpobj_p);
    }
    // debug -- log
    fprintf(stdout, "Playing song: %s; Starting at %d seconds.\n",
            ytpobj_p->title, ytpobj_p->start_time_offset);
    // copy the link into local buffer
    memcpy(link, ytpobj_p->audio_url, sizeof(ytpobj_p->audio_url));
    // get start_time_offset
    start_time_offset = ytpobj_p->start_time_offset;
    // clean up the peeked pointer
    ytpobj_p = NULL;
    sbuf_stop_peeking(&(yptr->media_player_t_ptr->song_queue));

    // start playing music
    yptr->media_player_t_ptr->current_song_time = 0;
    yptr->media_player_t_ptr->playing = 1;
    yptr->media_player_t_ptr->skippable = 1;
    send_websocket(
        yptr->vgt->voice_ssl,
        "{\"op\":5,\"d\":{\"speaking\":5,\"delay\":0,\"ssrc\":66666}}",
        strlen("{\"op\":5,\"d\":{\"speaking\":5,\"delay\":0,\"ssrc\":66666}}"),
        1);
    play_youtube_url(link, start_time_offset, yptr->key_str, yptr->ssrc_str,
                     yptr->dest_address, yptr->dest_port, yptr->socketfd,
                     yptr->cache_file_unique_name, yptr->media_player_t_ptr);

    // finish playing music, clean up and move on
    yptr->media_player_t_ptr->skippable = 0;
    yptr->media_player_t_ptr->playing = 0;
    sbuf_remove_end_value(&(yptr->media_player_t_ptr->song_queue), 0, 0, 0);
  }

  close(yptr->media_player_t_ptr->udp_fd);

  sbuf_deinit(&(yptr->media_player_t_ptr->song_queue));

  free(yptr->media_player_t_ptr->addr_malloc);
  free(yptr->key_str);
  free(yptr->ssrc_str);
  free(yptr->dest_address);
  free(yptr->dest_port);
  free(yptr->cache_file_unique_name);
  free(yptr->media_player_t_ptr);

  free(ptr);

  return NULL;
}

media_player_t *start_player(char *key_str, char *ssrc_str, char *dest_address,
                             char *dest_port, int socketfd,
                             char *cache_file_unique_name,
                             voice_gateway_t *vgt) {
  youtube_player_t *yptr = malloc(sizeof(youtube_player_t));
  int key_str_len = strlen(key_str) + 1;
  int ssrc_str_len = strlen(ssrc_str) + 1;
  int dest_address_len = strlen(dest_address) + 1;
  int dest_port_len = strlen(dest_port) + 1;
  int cache_file_unique_name_len = strlen(cache_file_unique_name) + 1;

  yptr->key_str = malloc(key_str_len);
  yptr->ssrc_str = malloc(ssrc_str_len);
  yptr->dest_address = malloc(dest_address_len);
  yptr->dest_port = malloc(dest_port_len);
  yptr->cache_file_unique_name = malloc(cache_file_unique_name_len);
  yptr->media_player_t_ptr = calloc(1, sizeof(media_player_t));
  yptr->media_player_t_ptr->addr_malloc = malloc(sizeof(struct sockaddr));

  yptr->socketfd = socketfd;

  memcpy(yptr->key_str, key_str, key_str_len);
  memcpy(yptr->ssrc_str, ssrc_str, ssrc_str_len);
  memcpy(yptr->dest_address, dest_address, dest_address_len);
  memcpy(yptr->dest_port, dest_port, dest_port_len);
  memcpy(yptr->cache_file_unique_name, cache_file_unique_name,
         cache_file_unique_name_len);

  sbuf_init(&(yptr->media_player_t_ptr->song_queue));
  sem_init(&(yptr->media_player_t_ptr->skipper), 0, 0);
  sem_init(&(yptr->media_player_t_ptr->quitter), 0, 0);
  sem_init(&(yptr->media_player_t_ptr->insert_song_mutex), 0, 1);
  sem_init(&(yptr->media_player_t_ptr->destination_info_mutex), 0, 1);
  sem_init(&(yptr->media_player_t_ptr->pauser), 0, 1);

  yptr->media_player_t_ptr->udp_fd = socketfd;
  yptr->media_player_t_ptr->ytp = yptr;

  yptr->vgt = vgt;

  pthread_t tid;
  pthread_create(&tid, NULL, media_player_threaded, yptr);
  yptr->media_player_t_ptr->player_thread_id = tid;

  return yptr->media_player_t_ptr;
}

media_player_t *modify_player(media_player_t *media, char *key_str_og,
                              char *ssrc_str, char *dest_address,
                              char *dest_port, int socketfd,
                              char *cache_file_unique_name,
                              voice_gateway_t *vgt) {
  close(media->udp_fd);

  sem_wait(&(media->destination_info_mutex));

  // fix key
  char *end;
  char *key_str_cpy = malloc(strlen(key_str_og) + 1);
  strcpy(key_str_cpy, key_str_og);
  char *key_str_cpy_unmov = key_str_cpy;

  int i = 0;
  while ((end = strchr(key_str_cpy, ','))) {
    *end = 0;
    media->key[i] = atoi(key_str_cpy);
    key_str_cpy = end + 1;
    i++;
  }
  media->key[31] = atoi(key_str_cpy);
  free(key_str_cpy_unmov);

  // fix file descriptor
  media->udp_fd = socketfd;

  // fix addr and addrlen
  int ret;
  struct addrinfo *addrs;
  struct addrinfo hints;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = 0;
  hints.ai_protocol = IPPROTO_UDP;
  ret = getaddrinfo(dest_address, dest_port, &hints, &addrs);
  if (ret != 0 || !addrs) {
    fprintf(stderr, "Cannot resolve host %s port %s: %s\n", dest_address,
            dest_port, gai_strerror(ret));
  }

  memcpy(media->addr_malloc, addrs->ai_addr, sizeof(struct sockaddr));
  media->addrlen = addrs->ai_addrlen;

  freeaddrinfo(addrs);

  // fix youtube player ptr values

  int key_str_len = strlen(key_str_og) + 1;
  int ssrc_str_len = strlen(ssrc_str) + 1;
  int dest_address_len = strlen(dest_address) + 1;
  int dest_port_len = strlen(dest_port) + 1;

  media->ytp->key_str = realloc(media->ytp->key_str, key_str_len);
  media->ytp->ssrc_str = realloc(media->ytp->ssrc_str, ssrc_str_len);
  media->ytp->dest_address =
      realloc(media->ytp->dest_address, dest_address_len);
  media->ytp->dest_port = realloc(media->ytp->dest_port, dest_port_len);

  strcpy(media->ytp->key_str, key_str_og);
  strcpy(media->ytp->ssrc_str, ssrc_str);
  strcpy(media->ytp->dest_address, dest_address);
  strcpy(media->ytp->dest_port, dest_port);
  media->ytp->socketfd = socketfd;

  sem_post(&(media->destination_info_mutex));

  send_websocket(
      media->ytp->vgt->voice_ssl,
      "{\"op\":5,\"d\":{\"speaking\":5,\"delay\":0,\"ssrc\":66666}}",
      strlen("{\"op\":5,\"d\":{\"speaking\":5,\"delay\":0,\"ssrc\":66666}}"),
      1);

  return 0;
}

int get_youtube_vid_info(char *query, youtube_page_object_t *ytobjptr) {
  int func_retval = 0;
  int pipeids[2];

  if (query == NULL) {
    printf("Error: Please provide a video ID...\n");
    exit(-1);
  }

  pipe(pipeids);

  pid_t pid;
  if ((pid = fork()) == 0) {
    close(pipeids[0]);
    dup2(pipeids[1], STDOUT_FILENO);

    char *argv[16];
    argv[0] = "youtube-dl";
    argv[1] = "--no-playlist";
    argv[2] = "--get-id";
    argv[3] = "-e";
    argv[4] = "--get-description";
    argv[5] = "--get-duration";
    argv[6] = "-g";
    argv[7] = "-f";
    argv[8] = ytobjptr->platform == PLATFORM_YOUTUBE ? FORMAT_M4A : FORMAT_MP3;
    argv[9] = query;
    argv[10] = 0;

    execvp(argv[0], argv);
  }
  close(pipeids[1]);
  int retval = 0;
  waitpid(pid, &retval, 0);
  if (retval) {
    func_retval = -1;
    goto CLEANUP_GETVIDINFO;
  }

  char str[sizeof(ytobjptr->title) + sizeof(ytobjptr->link) - 64 +
           sizeof(ytobjptr->description) + sizeof(ytobjptr->audio_url) +
           sizeof(ytobjptr->duration)];
  int len = read(pipeids[0], str, sizeof(str) - 2);
  if (len == -1) {
    func_retval = -1;
    goto CLEANUP_GETVIDINFO;
  }
  str[len] = 0;

  char *uid = strchr(str, '\n');
  if (!uid) {
    func_retval = -1;
    goto CLEANUP_GETVIDINFO;
  }
  *uid = 0;
  uid++;

  char *audio_url = strchr(uid, '\n');
  if (!audio_url) {
    func_retval = -1;
    goto CLEANUP_GETVIDINFO;
  }
  *audio_url = 0;
  audio_url++;

  char *desc = strchr(audio_url, '\n');
  if (!desc) {
    func_retval = -1;
    goto CLEANUP_GETVIDINFO;
  }
  *desc = 0;
  desc++;

  char *duration = strrchr(desc, '\n');
  if (!duration) {
    func_retval = -1;
    goto CLEANUP_GETVIDINFO;
  }
  *duration = 0;
  duration = strrchr(desc, '\n');
  if (!duration) {
    func_retval = -1;
    goto CLEANUP_GETVIDINFO;
  }
  *duration = 0;
  duration++;

  strncpy(ytobjptr->title, str, sizeof(ytobjptr->title) - 2);
  strncpy(ytobjptr->audio_url, audio_url, sizeof(ytobjptr->audio_url) - 2);

  if (ytobjptr->platform == PLATFORM_YOUTUBE) {
    snprintf(ytobjptr->link, sizeof(ytobjptr->link),
             "https://www.youtube.com/watch?v=%s", uid);
  } else if (ytobjptr->platform == PLATFORM_SOUNDCLOUD) {
    snprintf(ytobjptr->link, sizeof(ytobjptr->link),
             "https://w.soundcloud.com/player/?url=https%%3A//"
             "api.soundcloud.com/tracks/%s",
             uid);
  }

  strncpy(ytobjptr->description, desc, sizeof(ytobjptr->description) - 2);
  strncpy(ytobjptr->duration, duration, sizeof(ytobjptr->duration) - 2);

  char *time_sep = strchr(duration, ':');
  if (!time_sep) {
    ytobjptr->length_in_seconds = atoi(duration);
  } else {
    *time_sep = 0;
    time_sep++;
    char *time_sep2 = strchr(time_sep, ':');
    if (!time_sep2) {
      ytobjptr->length_in_seconds = 60 * atoi(duration) + atoi(time_sep);
    } else {
      *time_sep2 = 0;
      time_sep2++;
      ytobjptr->length_in_seconds =
          60 * 60 * atoi(duration) + 60 * atoi(time_sep) + atoi(time_sep2);
    }
  }

  fprintf(stdout, "Video information resolved: %s\n", ytobjptr->title);

  clock_gettime(CLOCK_REALTIME, &(ytobjptr->audio_url_create_date));

CLEANUP_GETVIDINFO:

  close(pipeids[0]);

  return func_retval;
}

//-1 index signifies insert at front
// positive index will start from the back.
// index should not be zero
int insert_queue_ydl_query(media_player_t *media, char *ydl_query,
                           char *return_title, int return_title_len, int index,
                           int platform) {
  media->skippable = 1;

  sem_wait(&(media->insert_song_mutex)); // necessary to fix -leave cmd

  youtube_page_object_t ytobj = {0};
  ytobj.platform = platform;

  strncpy(ytobj.query, ydl_query, sizeof(ytobj.query) - 2);
  int ret = get_youtube_vid_info(ydl_query, &ytobj);

  if (!ret) {
    if (index == -1) {
      sbuf_insert_front_value((&(media->song_queue)), &ytobj, sizeof(ytobj));
    } else if (index > 0) {
      int queue_size = media->song_queue.size;
      int effective_index = index > queue_size ? queue_size : index;
      sbuf_insert_value_position_from_back((&(media->song_queue)), &ytobj,
                                           sizeof(ytobj), effective_index);
    }
  }

  sem_post(&(media->insert_song_mutex));

  strncpy(return_title, ytobj.title, return_title_len - 1);
  return (!ret) - 1;
}

// finish object evaluation for necessary informations
void complete_youtube_object_fields(youtube_page_object_t *ytobjptr) {
  get_youtube_vid_info(ytobjptr->link, ytobjptr);
}

void seek_media_player(media_player_t *media, int time_in_seconds) {
  youtube_page_object_t ytpobj;
  void *retval = sbuf_peek_end_value_copy(&(media->song_queue), &(ytpobj),
                                          sizeof(ytpobj), 0);
  if (!retval) {
    return;
  }

  if (time_in_seconds < 0 || time_in_seconds >= ytpobj.length_in_seconds) {
    time_in_seconds = 0;
  }

  ytpobj.start_time_offset = time_in_seconds;

  sbuf_insert_value_position_from_back(&(media->song_queue), &ytpobj,
                                       sizeof(ytpobj), 1);
  sem_post(&(media->skipper));
}

void shuffle_media_player(media_player_t *media) {
  sbuf_shuffle_random(&(media->song_queue));
}

void clear_media_player(media_player_t *media) {
  youtube_page_object_t ytpobj;
  void *retval = sbuf_peek_end_value_copy(&(media->song_queue), &(ytpobj),
                                          sizeof(ytpobj), 0);
  if (!retval) {
    return;
  }

  sbuf_clear(&(media->song_queue));
  sbuf_insert_value_position_from_back(&(media->song_queue), &ytpobj,
                                       sizeof(ytpobj), 0);
}

// Playlists
/* ----------------------------- FETCH PLAYLIST ----------------------------- */

// Video playlist (where the link also contains a video) json macros
#define VIDEO_JSON_KEYS_LEN 5
#define VIDEO_JSON_KEYS                                                        \
  {                                                                            \
    "contents", "twoColumnWatchNextResults", "playlist", "playlist",           \
        "contents"                                                             \
  }
#define VIDEO_JSON_DATA_KEY "playlistPanelVideoRenderer"

// Playlist page json macros
#define PAGE_JSON_KEYS_LEN 14
#define PAGE_JSON_KEYS                                                         \
  {                                                                            \
    "contents", "twoColumnBrowseResultsRenderer", "tabs", "\0", "tabRenderer", \
        "content", "sectionListRenderer", "contents", "\0",                    \
        "itemSectionRenderer", "contents", "\0", "playlistVideoListRenderer",  \
        "contents"                                                             \
  }
#define PAGE_JSON_DATA_KEY "playlistVideoRenderer"

// Error codes
#define NOT_IMPLEMENTED_ERR -1
#define FETCH_ERR 1
#define TRIM_ERR 2
#define LEN_ERR 3
#define KEY_ERR 4 // Happens with invalid playlist IDs
#define JSON_ERR 5

int fetch_youtube_playlist(char *url, int start, media_player_t *media,
                           char *title, int title_len) {

  // Fetch html
  char *html = NULL;
  cJSON *video_json_list = NULL;
  int ret = 0;

  int fetch_err = fetch_get(url, &html);
  if (fetch_err) {
    ret = FETCH_ERR;
    goto PLAYLIST_FETCH_CLEANUP;
  }

  // Search for data json and trim
  int trim_err = trim_between(html, "ytInitialData = ", ";</script>");
  if (trim_err) {
    ret = TRIM_ERR;
    goto PLAYLIST_FETCH_CLEANUP;
  }

  // Check if playlist page or video playlist
  int playlist_page = strstr(url, "/playlist") != NULL;

  // Get inner video list
  video_json_list = cJSON_Parse(html);
  cJSON *inner_json = video_json_list;

  char *playlist_title = NULL;

  // Navigate into inner json
  if (playlist_page) {
    char keys[PAGE_JSON_KEYS_LEN][50] = PAGE_JSON_KEYS;

    playlist_title = cJSON_GetStringValue(cJSON_GetObjectItem(
        cJSON_GetObjectItem(cJSON_GetObjectItem(inner_json, "metadata"),
                            "playlistMetadataRenderer"),
        "title"));

    for (int i = 0; i < PAGE_JSON_KEYS_LEN; i++) {
      if (!keys[i][0]) {
        inner_json = cJSON_GetArrayItem(inner_json, 0);
      } else {
        inner_json = cJSON_GetObjectItem(inner_json, keys[i]);
      }
    }

  } else {
    char keys[VIDEO_JSON_KEYS_LEN][50] = VIDEO_JSON_KEYS;

    for (int i = 0; i < VIDEO_JSON_KEYS_LEN; i++) {
      inner_json = cJSON_GetObjectItem(inner_json, keys[i]);

      if (strncmp(keys[i], "playlist", 8) == 0 && !playlist_title) {
        playlist_title = cJSON_GetStringValue(cJSON_GetObjectItem(
            cJSON_GetObjectItem(inner_json, "playlist"), "title"));
      }
    }

    // Invalid playlist IDs will be missing keys here
    if (!inner_json) {
      ret = KEY_ERR;
      goto PLAYLIST_FETCH_CLEANUP;
    }
  }

  snprintf(title, title_len + 1, "%s", playlist_title);

  int len = cJSON_GetArraySize(inner_json);

  // Insert each video
  for (int i = start; i < len; i++) {
    cJSON *data = cJSON_GetArrayItem(inner_json, i);

    // Skip if the last video json is a continuation link rather than a video
    if (cJSON_HasObjectItem(data, "continuationItemRenderer")) {
      continue;
    }

    cJSON *video = cJSON_GetObjectItem(
        data, playlist_page ? PAGE_JSON_DATA_KEY : VIDEO_JSON_DATA_KEY);

    // Get key values
    youtube_page_object_t ytb_obj = {0};

    char *id = cJSON_GetStringValue(cJSON_GetObjectItem(video, "videoId"));

    char *song_title;
    if (playlist_page) {
      song_title = cJSON_GetStringValue(cJSON_GetObjectItem(
          cJSON_GetArrayItem(
              cJSON_GetObjectItem(cJSON_GetObjectItem(video, "title"), "runs"),
              0),
          "text"));
    } else {
      song_title = cJSON_GetStringValue(cJSON_GetObjectItem(
          cJSON_GetObjectItem(video, "title"), "simpleText"));
    }

    char *duration = cJSON_GetStringValue(cJSON_GetObjectItem(
        cJSON_GetObjectItem(video, "lengthText"), "simpleText"));

    if (!duration) {
      ret = LEN_ERR;
      goto PLAYLIST_FETCH_CLEANUP;
    }

    int time = parse_time(duration);

    snprintf(ytb_obj.link, sizeof(ytb_obj.link) - 2,
             "https://www.youtube.com/watch?v=%s", id);
    strncpy(ytb_obj.title, song_title, sizeof(ytb_obj.title) - 2);
    strncpy(ytb_obj.duration, duration, sizeof(ytb_obj.duration) - 2);
    ytb_obj.length_in_seconds = time;

    media->skippable = 1;

    sbuf_insert_front_value((&(media->song_queue)), &ytb_obj, sizeof(ytb_obj));
  }

PLAYLIST_FETCH_CLEANUP:
  if (video_json_list != NULL)
    cJSON_Delete(video_json_list);
  if (html != NULL)
    free(html);

  return ret;
}

int fetch_soundcloud_playlist(char *url, int start, media_player_t *media,
                              char *title, int title_len) {

  // Fetch html
  char *html = NULL;
  int ret = 0;
  cJSON *json = NULL;
  char *playlist_title = NULL;

  int fetch_err = fetch_get(url, &html);
  if (fetch_err) {
    ret = FETCH_ERR;
    goto SC_PLAYLIST_FETCH_CLEANUP;
  }

  // Search for data json and trim
  int trim_err =
      trim_between(html, "<script>window.__sc_hydration = ", ";</script>");
  if (trim_err) {
    ret = TRIM_ERR;
    goto SC_PLAYLIST_FETCH_CLEANUP;
  }

  // Check if playlist page or video playlist
  int embedded_page = strstr(url, "?in=") != NULL;

  if (embedded_page) {
    ret = NOT_IMPLEMENTED_ERR;
    goto SC_PLAYLIST_FETCH_CLEANUP;
  }

  json = cJSON_Parse(html);

  for (int i = 0; i < cJSON_GetArraySize(json); i++) {
    cJSON *array_item = cJSON_GetArrayItem(json, i);

    if (!strncmp(
            cJSON_GetStringValue(cJSON_GetObjectItem(array_item, "hydratable")),
            "playlist", 8)) {

      cJSON *tracks_arr = cJSON_GetObjectItem(
          cJSON_GetObjectItem(array_item, "data"), "tracks");

      for (int i = 0; i < cJSON_GetArraySize(tracks_arr); i++) {
        cJSON *track = cJSON_GetArrayItem(tracks_arr, i);

        youtube_page_object_t ytb_obj = {0};

        ytb_obj.platform = PLATFORM_SOUNDCLOUD;

        if (cJSON_HasObjectItem(track, "permalink_url")) {
          strncpy(
              ytb_obj.link,
              cJSON_GetStringValue(cJSON_GetObjectItem(track, "permalink_url")),
              sizeof(ytb_obj.link) - 2);

          if (cJSON_HasObjectItem(track, "title")) {
            strncpy(ytb_obj.title,
                    cJSON_GetStringValue(cJSON_GetObjectItem(track, "title")),
                    sizeof(ytb_obj.title) - 2);
          }

          if (cJSON_HasObjectItem(track, "duration")) {
            double length =
                cJSON_GetNumberValue(cJSON_GetObjectItem(track, "duration"));

            parse_duration(length, ytb_obj.duration);

            ytb_obj.length_in_seconds = (int)length / 1000;
          }
        } else {
          // Some soundcloud tracks' json only contains an id

          // FIXME These json contains unresolved fields, which takes long to
          // load
          snprintf(ytb_obj.link, sizeof(ytb_obj.link),
                   "https://w.soundcloud.com/player/?url=https%%3A//"
                   "api.soundcloud.com/tracks/%d",
                   (int)cJSON_GetNumberValue(cJSON_GetObjectItem(track, "id")));
        }

        media->skippable = 1;

        sbuf_insert_front_value((&(media->song_queue)), &ytb_obj,
                                sizeof(ytb_obj));
      }

      playlist_title = cJSON_GetStringValue(cJSON_GetObjectItem(
          cJSON_GetObjectItem(array_item, "data"), "title"));

      if (playlist_title == NULL) {
        ret = JSON_ERR;
        goto SC_PLAYLIST_FETCH_CLEANUP;
      }

      strncpy(title, playlist_title, title_len - 2);

      break;
    }
  }

SC_PLAYLIST_FETCH_CLEANUP:
  if (json != NULL)
    cJSON_Delete(json);
  if (html != NULL)
    free(html);

  return ret;
}
