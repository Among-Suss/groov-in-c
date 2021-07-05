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

#include "media.h"

#define MAX_URL_LEN 16384

// helpers from opusrtp.c

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
  // unsigned char *packet, *opus_encrypted_pack; //DANGEROUS
  int ret;
  unsigned char packet[65535];

  update_rtp_header(rtp);
  serialize_rtp_header(packet, rtp->header_size, rtp);

  // ENCRYPT HERE ___ENCRYPT OPUS PACKET___opus_packet
  unsigned char nonce[24];
  memcpy(nonce, packet, 12);
  memset(nonce + 12, 0, 12);

  // opus_encrypted_pack = malloc(rtp->payload_size +
  // crypto_secretbox_MACBYTES); //DANGEROUS
  crypto_secretbox_easy(packet + rtp->header_size, opus_packet,
                        rtp->payload_size, nonce, key);

  ret = sendto(fd, packet,
               rtp->header_size + rtp->payload_size + crypto_secretbox_MACBYTES,
               0, addr, addrlen);

  // DANGEROUS////////////

  char buffer[100000];
  struct sockaddr_storage src_addr;
  socklen_t src_addr_len = sizeof(src_addr);
  int cnt = 0;
  // cnt = recvfrom(fd,buffer,sizeof(buffer),0,(struct
  // sockaddr*)&src_addr,&src_addr_len);

  // fprintf(stderr, "RECEIVED:::::::%d\n", cnt);
  ///////////////////////

  if (ret < 0) {
    fprintf(stderr, "error sending: %s\n", strerror(errno));
  }
  // free(packet);  //DANGEROUS
  // free(opus_encrypted_pack);   //DANGEROUS

  return ret;
}

int rtp_send_file_to_addr(const char *filename, struct sockaddr *addr,
                          socklen_t addrlen, int payload_type,
                          unsigned char *key, int ssrc,
                          int *ffmpeg_running_state) {
  /* POSIX MONOTONIC CLOCK MEASURER */
  clockid_t clock_id;
  struct timespec start, start2, end;
  sysconf(_SC_MONOTONIC_CLOCK);
  clock_id = CLOCK_MONOTONIC;
  clock_gettime(clock_id, &start);
  int last_frame_corrected = 0;

  time_slot_wait_t state;
  init_time_slot_wait(&state);

  rtp_header rtp;
  int fd;
  int optval = 0;
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

  // danger
  struct timeval read_timeout;
  read_timeout.tv_sec = 0;
  read_timeout.tv_usec = 10;
  ////////

  fd = socket(addr->sa_family, SOCK_DGRAM, IPPROTO_UDP);
  // check for fd < 0 Couldn't create socket
  ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));
  // check ret < 0 Couldn't set socket options

  // DANGER
  ret = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &read_timeout,
                   sizeof(read_timeout));
  ////////

  rtp.version = 2;
  rtp.type = payload_type;
  rtp.pad = 0;
  rtp.ext = 0;
  rtp.cc = 0;
  rtp.mark = 0;
  rtp.seq = rand();
  rtp.time = rand();
  rtp.ssrc = ssrc; //= rand();  ////MODIFIED FROM ORIGINAL !!!!DANGEROUS
  rtp.csrc = NULL;
  rtp.header_size = 0;
  rtp.payload_size = 0;

  fprintf(stderr, "Starting sender\n\nSending %s\n\n", filename);
  in_fd = open(filename, O_RDONLY, 0644);
  // check !in Couldn't open input file

  ret = ogg_sync_init(&oy);
  // check ret < 0 Couldn't initialize Ogg sync state

  while ((read_test_len = read(in_fd, &dummy_char, 1)) ||
         *ffmpeg_running_state) {

    if (read_test_len == 1) {
      lseek(in_fd, -1, SEEK_CUR);
    }else{
      struct timespec sleeptime;
      sleeptime.tv_nsec = 20000000;
      sleeptime.tv_sec = 0;
      nanosleep(&sleeptime, NULL);
      continue;
    }

    // printf("eof: %d\n", !read_test_len);
    in_data = ogg_sync_buffer(&oy, in_size);
    // check !in_data ogg_sync_buffer failed

    in_read = read(in_fd, in_data, in_size);
    ret = ogg_sync_wrote(&oy, in_read);
    // check ret < 0 ogg_sync_wrote failed

    while (ogg_sync_pageout(&oy, &og) == 1) {
      if (headers == 0) {
        if (is_opus(&og)) {
          /* this is the start of an Opus stream */
          ret = ogg_stream_init(&os, ogg_page_serialno(&og));
          // check ret < 0 ogg_stream_init failed
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
      // check ret < 0 ogg_stream_pagein failed

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
        rtp.seq++;
        rtp.time += samples;
        rtp.payload_size = op.bytes;
        // fprintf(stderr, "rtp %d %d %d %3d ms %5d bytes\n",
        //    rtp.type, rtp.seq, rtp.time, samples/48, rtp.payload_size);
        send_rtp_packet(fd, addr, addrlen, &rtp, op.packet, key);

        clock_gettime(clock_id, &end);
        long secediff = (long int)end.tv_sec - start.tv_sec;
        long usecdiff =
            (long)((end.tv_nsec - start.tv_nsec) / 1000) + secediff * 1000000;
        // fprintf(stderr, "Time elapsed: %ld, should be:%ld us", usecdiff,
        // samples*125/6);

        if (usecdiff > 25000 || usecdiff < 15000) {
          fprintf(stderr,
                  "WHOOPS...Abnormal Frame: "
                  "........................................................... "
                  "%ld\n",
                  usecdiff);
        } /*else{
           fprintf(stderr, "\n");
         }*/
        start = end;

        long target = samples * 62500 / 3;

        long lastnsdiff = usecdiff * 1000;
        long deviation = lastnsdiff - target;
        if (!last_frame_corrected &&
            ((deviation > 5000000 && deviation < 15000000) ||
             (deviation < -5000000 && deviation > -15000000))) {
          target = target - deviation;
          last_frame_corrected = 1;
        } else {
          last_frame_corrected = 0;
        }

        // long secdiff2 = end.tv_sec - start2.tv_sec;
        // long nsecdiff = end.tv_nsec - start2.tv_nsec + (secdiff2 *
        // 1000000000);
        /*
        while (nsecdiff < target){

          //todo: find some way to "sleep"

          clock_gettime(clock_id, &end);
          secdiff2 = end.tv_sec - start2.tv_sec;
          nsecdiff = end.tv_nsec - start2.tv_nsec + (secdiff2 * 1000000000);
        }
        */
        // fprintf(stderr, "raw time: %ld\n", nsecdiff);

        /* convert number of 48 kHz samples to nanoseconds without overflow */
        wait_for_time_slot(samples * 62500 / 3, &state);

        // clock_gettime(clock_id, &start2);
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
                  int *ffmpeg_running_state) {
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
  ret = rtp_send_file_to_addr(filename, addrs->ai_addr, addrs->ai_addrlen,
                              payload_type, key, ssrc, ffmpeg_running_state);
  freeaddrinfo(addrs);
  return ret;
}

void get_youtube_audio_url(char *video_id, char *url) {
  int pipeids[2];

  if (video_id == NULL) {
    printf("Error: Please provide a video ID...\n");
    exit(-1);
  }

  pipe(pipeids);

  pid_t pid;
  if ((pid = fork()) == 0) {
    close(pipeids[0]);
    dup2(pipeids[1], STDOUT_FILENO);

    char *argv[10];
    argv[0] = "youtube-dl";
    argv[1] = "-g";
    argv[2] = "-f";
    argv[3] = "bestaudio[ext=m4a]";
    argv[4] = video_id;
    argv[5] = 0;

    execvp(argv[0], argv);
  }
  close(pipeids[1]);
  char str[MAX_URL_LEN];
  int len = read(pipeids[0], str, MAX_URL_LEN);
  str[len] = 0;
  close(pipeids[0]);
  waitpid(pid, NULL, 0);

  char *urlendp = strstr(str, "\n");
  *urlendp = 0;
  strcpy(url, str);

  printf("AUDIO URL: %s\n\n", url);
}

void *ffmpeg_process_waiter(void *ptr) {
  pthread_detach(pthread_self());
  ffmpeg_process_waiter_t *fptr = (ffmpeg_process_waiter_t *)ptr;
  pid_t pid = fptr->pid;
  int *ffmpeg_process_state = fptr->ffmpeg_process_state;
  free(ptr);

  while (waitpid(pid, NULL, 0) < 0)
    ;

  *ffmpeg_process_state = 0;

  return NULL;
}

void play_youtube_url(char *youtube_link, char *key_str, char *ssrc_str,
                      char *dest_address, char *dest_port,
                      char *cache_file_unique_name) {
  char url[MAX_URL_LEN];

  int fd = open(cache_file_unique_name, O_CREAT | O_RDWR | O_TRUNC, 0644);
  close(fd);

  get_youtube_audio_url(youtube_link, url);

  pid_t pid;
  if ((pid = fork()) == 0) {
    char *new_argv[50] = {"ffmpeg",

                          "-ss",
                          "00:00:00.00",

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
      printf("UNIX EXECVE ERROR\n");
      exit(1);
    }
  }

  int ffmpeg_process_state_value = 1;
  ffmpeg_process_waiter_t *fptr = malloc(sizeof(ffmpeg_process_waiter_t));

  fptr->pid = pid;
  fptr->ffmpeg_process_state = &ffmpeg_process_state_value;

  pthread_t tid;
  pthread_create(&tid, NULL, ffmpeg_process_waiter, fptr);

  unsigned char diskey[32];
  char *end;

  int i = 0;
  while ((end = strchr(key_str, ','))) {
    *end = 0;
    diskey[i] = atoi(key_str);
    key_str = end + 1;
    i++;
  }
  diskey[31] = atoi(key_str);

  int ssrc = atoi(ssrc_str);

  rtp_send_file(cache_file_unique_name, dest_address, dest_port, 120, diskey,
                ssrc, &ffmpeg_process_state_value);
}

void *play_yt_threaded(void *ptr) {
  pthread_detach(pthread_self());
  youtube_player_t *yptr = (youtube_player_t *)ptr;

  play_youtube_url(yptr->youtube_link, yptr->key_str, yptr->ssrc_str,
                   yptr->dest_address, yptr->dest_port,
                   yptr->cache_file_unique_name);

  
  free(yptr->youtube_link);
  free(yptr->key_str);
  free(yptr->ssrc_str);
  free(yptr->dest_address);
  free(yptr->dest_port);
  free(yptr->cache_file_unique_name);
  
  free(ptr);

  return NULL;
}

void play_youtube_in_thread(char *youtube_link, char *key_str, char *ssrc_str,
                            char *dest_address, char *dest_port,
                            char *cache_file_unique_name) {

  youtube_player_t *yptr = malloc(sizeof(youtube_player_t));
  int youtube_link_len = strlen(youtube_link) + 1;
  int key_str_len = strlen(key_str) + 1;
  int ssrc_str_len = strlen(ssrc_str) + 1;
  int dest_address_len = strlen(dest_address) + 1;
  int dest_port_len = strlen(dest_port) + 1;
  int cache_file_unique_name_len = strlen(cache_file_unique_name) + 1;

  yptr->youtube_link = malloc(youtube_link_len);
  yptr->key_str = malloc(key_str_len);
  yptr->ssrc_str = malloc(ssrc_str_len);
  yptr->dest_address = malloc(dest_address_len);
  yptr->dest_port = malloc(dest_port_len);
  yptr->cache_file_unique_name = malloc(cache_file_unique_name_len);

  memcpy(yptr->youtube_link, youtube_link, youtube_link_len);
  memcpy(yptr->key_str, key_str, key_str_len);
  memcpy(yptr->ssrc_str, ssrc_str, ssrc_str_len);
  memcpy(yptr->dest_address, dest_address, dest_address_len);
  memcpy(yptr->dest_port, dest_port, dest_port_len);
  memcpy(yptr->cache_file_unique_name, cache_file_unique_name, cache_file_unique_name_len);

  pthread_t tid;
  pthread_create(&tid, NULL, play_yt_threaded, yptr);
}
