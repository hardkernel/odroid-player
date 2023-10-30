/*
 * Copyright (C) 2021 Amlogic Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include "agmplayer.h"

#define FALSE 0
#define TRUE 1

const int FILE_NUM_MAX = 1024;
typedef struct
{
  char* uris[1024];
  unsigned int filenum;
  int cur_index;
} FILE_LISE;
static FILE_LISE file_list = { {0}, 0, 0};

static bool quiet = FALSE;
static bool player_quit = FALSE;

static bool play_next (AGMP_HANDLE handle);
static bool play_prev (AGMP_HANDLE handle);
static void agmp_message_callback(AGMP_HANDLE handle, AGMP_MESSAGE_TYPE type, void* userdata);

static void
gst_play_printf (const char * format, ...)
{
  char *str = NULL;
  va_list args;
  int len;

  if (quiet)
    return;

  va_start (args, format);

  len = g_vasprintf (&str, format, args);

  va_end (args);

  if (len > 0 && str != NULL)
    printf ("%s", str);

  g_free (str);
}

#define gst_print gst_play_printf
#define SECOND (1000)

static void collect_media_info(AGMP_HANDLE handle)
{
  //duration
  long dur = agmp_get_duration(handle);
  gst_print ("%ld:%02ld:%02ld.%03ld", dur/SECOND/3600, dur/SECOND/60%60, dur/SECOND%60, dur%SECOND);
  //track number
  int n_video=0, n_audio=0, n_text=0;
  aamp_get_media_track_num(handle, &n_video, &n_audio, &n_text);
  //video
  for (int i=0; i < n_video; i++) {
    VideoInfo video_info = {0};
    aamp_get_video_track_info(handle, i, &video_info);
    gst_print ("video stream:%d, codec:%s, container:%s, width:%d, height:%d, framerate:%d\n", \
      video_info.track_id, video_info.codec, video_info.container, video_info.width, video_info.height, video_info.framerate);
  }
  //audio
  for (int i=0; i < n_audio; i++) {
    AudioInfo audio_info = {0};
    aamp_get_audio_track_info(handle, i, &audio_info);
    gst_print ("audio stream:%d, codec:%s, container:%s, samples:%d, channels:%d, rate:%d\n", \
      audio_info.track_id, audio_info.codec, audio_info.container, audio_info.samples, audio_info.channels, audio_info.rate);
  }
  //text
  for (int i=0; i < n_text; i++) {
    TextInfo text_info = {0};
    aamp_get_text_track_info(handle, i, &text_info);
    gst_print ("subtitle stream:%d, language:%s\n", text_info.track_id, text_info.lang);
  }
}

static void agmp_message_callback(AGMP_HANDLE handle, AGMP_MESSAGE_TYPE type, void* userdata)
{
  switch (type)
  {
    case AGMP_MESSAGE_BUFFERING:
      gst_print ("message BUFFERING: %d%%.....\r", agmp_get_buffering_percent(handle));
    break;
    case AGMP_MESSAGE_ASYNC_DONE:
    {
      gst_print ("message ASYNC_DONE.....\n");
      collect_media_info(handle);
    }
    break;
    case AGMP_MESSAGE_EOS:
      gst_print ("message EOS.....\n");
      if (!play_next (handle)) {
        gst_print ("reach the filelist end, stop.\n");
        agmp_stop (handle);
        player_quit=TRUE;
      }
    break;
    case AGMP_MESSAGE_ERROR:
      gst_print ("message ERROR.....\n");
      if (!play_next (handle)) {
        gst_print ("reach the filelist end, stop.\n");
        agmp_stop (handle);
        player_quit=TRUE;
      }
    break;
    case AGMP_MESSAGE_VIDEO_UNDERFLOW:
      gst_print ("message AGMP_MESSAGE_VIDEO_UNDERFLOW.....\n");
    break;
    case AGMP_MESSAGE_AUDIO_UNDERFLOW:
      gst_print ("message AGMP_MESSAGE_AUDIO_UNDERFLOW.....\n");
    break;
    case AGMP_MESSAGE_FIRST_VFRAME:
      gst_print ("message AGMP_MESSAGE_FIRST_VFRAME.....\n");
    break;
    case AGMP_MESSAGE_FIRST_AFRAME:
      gst_print ("message AGMP_MESSAGE_FIRST_AFRAME.....\n");
    break;
    case AGMP_MESSAGE_MEDIA_INFO_CHANGED:
      gst_print ("message AGMP_MESSAGE_MEDIA_INFO_CHANGED.....\n");
    break;
  }
}

#define INPUT_MAX_LEN 1024
static void trim(char *cmd)
{
  //remove space
  if (cmd[0] == ' ')
  {
    for (int i=0; i<strlen(cmd); i++)
    {
      if (cmd[i] != ' ')
      {
          memmove(cmd, &cmd[i], strlen(cmd)-i);
          cmd[strlen(cmd)-i] = '\0';
          break;
      }
    }
  }

  //remove space \n \r. if there is only one char, it is not effective.
  for (int i = strlen(cmd) - 1; i >= 0; i--)
  {
    if (cmd[i] != ' ' && cmd[i] != '\r' && cmd[i] != '\n')
    {
      cmd[i+1] = '\0';
      break;
    }
  }
}

#define DEBUG_TEST_CMD 0

AGMP_PLAY_SPEED play_speed = AGMP_PLAY_SPEED_1;
static void run_command (void* user_data)
{
  double value = 0;
  AGMP_HANDLE handle = (AGMP_HANDLE) user_data;
  char cmd[INPUT_MAX_LEN] = {0};
  int x = 0, y = 0, w = 0, h = 0;
  while (!player_quit)
  {
    //copy cmd and trim
    gst_print("agmplayer> ");
    char *ret = fgets(cmd, sizeof(cmd), stdin);
    trim(cmd);
    gst_print ("cmd:%s\n", cmd);

    if (sscanf(cmd, "seek %lf", &value) >= 1)
    {
      agmp_seek(handle, value);
    }
    else if (strcmp(cmd, "stop") == 0)
    {
      gst_print ("\nstop............\n");
      agmp_stop (handle);
    }
    else if (strcmp(cmd, "exit") == 0)
    {
      gst_print ("\nexit............\n");
      /* clean up */
      player_quit=TRUE;
      break;
    }
    else if (strcmp(cmd, "play") == 0)
    {
      gst_print ("\nplay............\n");
      agmp_play (handle);
    }
    else if (strcmp(cmd, "pause") == 0)
    {
      gst_print ("pause\n");
      agmp_pause (handle);
    }
    else if (strcmp(cmd,"next")==0)
    {
        play_next(handle);
    }
    else if (strcmp(cmd,"prev")==0)
    {
      play_prev(handle);
    }
    else if (sscanf(cmd, "vol %lf", &value) >= 1)
    {
      agmp_set_volume(handle, value);
    }
    else if (strcmp(cmd,"+")==0)
    {
      if (++play_speed > AGMP_PLAY_SPEED_8) {
        play_speed = AGMP_PLAY_SPEED_8;
      }
      agmp_set_speed(handle, play_speed);
    }
    else if (strcmp(cmd,"-")==0)
    {
      if (--play_speed < AGMP_PLAY_SPEED_1_2) {
        play_speed = AGMP_PLAY_SPEED_1_4;
      }
      agmp_set_speed(handle, play_speed);
    }
    else if (sscanf(cmd, "atrack %lf", &value) >= 1)
    {
      aamp_set_audio_track(handle, value);
    }
    else if (sscanf(cmd, "vtrack %lf", &value) >= 1)
    {
    }
    else if (sscanf(cmd, "strack %lf", &value) >= 1)
    {
    }
    else if (strcmp(cmd,"0")==0)
    {
      agmp_stop (handle);
      agmp_prepare(handle);
      agmp_play (handle);
    }
#if DEBUG_TEST_CMD
    else if (strcmp(cmd,"getvol")==0)
    {
      gst_print ("agmp_get_volume............%f\n", agmp_get_volume(handle));
    }
    else if (strcmp(cmd,"getspeed")==0)
    {
      gst_print ("agmp_get_speed............%d\n", agmp_get_speed(handle));
    }
    else if (strcmp(cmd,"getwindowsize")==0)
    {
      agmp_get_window_size(handle, &x, &y, &w, &h);
      gst_print ("agmp_get_window_size............[%d,%d,%d,%d]\n", x, y, w, h);
    }
    else if (sscanf(cmd, "zoom %lf", &value) >= 1)
    {
      agmp_set_zoom(handle, value);
      gst_print ("agmp_set_zoom............[%d]\n", (int)value);
    }
    else if (sscanf(cmd, "vmute %lf", &value) >= 1)
    {
      agmp_set_video_mute(handle, value);
      gst_print ("agmp_set_video_mute............[%d]\n", (int)value);
    }
    else if (sscanf(cmd, "setwindowsize %d,%d,%d,%d", &x, &y, &w, &h) >= 1)
    {
      agmp_set_window_size(handle, x, y, w, h);
    }
#endif
    else
    {

    }
  }
}


/* returns FALSE if we have reached the end of the playlist */
static bool play_next (AGMP_HANDLE handle)
{
  if (++file_list.cur_index >= file_list.filenum)
  {
    gst_printerr ("reached the end of the playlist!!!\n");
	  return FALSE;
  }
  agmp_stop (handle);
  agmp_set_uri(handle, file_list.uris[file_list.cur_index]);
  //agmp_set_license_url(handle, license_url);
  agmp_prepare(handle);
  agmp_play (handle);
  return TRUE;
}

/* returns FALSE if we have reached the beginning of the playlist */
static bool play_prev (AGMP_HANDLE handle)
{
  if (--file_list.cur_index < 0)
  {
    gst_printerr ("reached the beginning of the playlist!!!\n");
	  return FALSE;
  }
  agmp_stop (handle);
  agmp_set_uri(handle, file_list.uris[file_list.cur_index]);
  //agmp_set_license_url(handle, license_url);
  agmp_prepare(handle);
  agmp_play (handle);
  return TRUE;
}

static parse_param(char** args)
{

}

int main (int argc, char **argv)
{
  char license_url[1024] = {0};
  int x=0, y=0, w=0, h=0;

  if (argc < 2)
  {
	gst_printerr ("Usage: %s FILE1|URI1 [FILE2|URI2] [FILE3|URI3] ...", "testbin");
    gst_printerr ("\n\n");
    gst_printerr ("%s\n\n", "You must provide at least one filename or URI to play.");
  }

  file_list.filenum = 0;
  file_list.cur_index = 0;
  memset(file_list.uris, 0,FILE_NUM_MAX);
  for (int i = 1; i < argc; i++) {
    if (strncmp ( argv[i], "--", 2 )) {
      if (file_list.filenum >= FILE_NUM_MAX) {
        gst_printerr ("the number of filename is out of range\n");
        continue;
      }
      file_list.uris[file_list.filenum++] = argv[i];
      gst_print ("got uri: %s\n", argv[i]);
    }
    else {
      if (sscanf(argv[i], "--license-url=%s", &license_url) >= 1)
      {
        gst_print ("got license-url: %s\n", license_url);
      }
      else if(sscanf(argv[i], "--window-size=%d,%d,%d,%d", &x, &y, &w, &h) >= 1)
      {
        gst_print ("got window-size=%d,%d,%d,%d\n", x, y, w, h);
      }
      else {
        gst_print ("unknown param: %s\n", argv[i]);
        return 0;
      }
    }
  }

  gst_print ("found source: %d\n", file_list.filenum);
  if (NULL == file_list.uris[0])
  {
    gst_printerr ("Usage: %s FILE1|URI1 [FILE2|URI2] [FILE3|URI3] ...\n", "testbin");
    gst_printerr ("please input url...\n");
    return 0;
  }

  /* prepare */
  AGMP_HANDLE handle = agmp_init();
  if (handle == NULL) {
    gst_printerr
        ("Failed to create 'playbin' element. Check your GStreamer installation.\n");
    return EXIT_FAILURE;
  }
  agmp_set_log_level(LOG_TRACE);

  player_quit = FALSE;
  pthread_t cmdThreadId;
  if (pthread_create(&cmdThreadId,NULL,run_command, handle) != 0)
  {
    printf("[AAMPCLI] Failed at create pthread error\n");
  }
  aamp_register_events(handle, agmp_message_callback, NULL);
  file_list.cur_index = 0;
  gst_print ("\n uris %s, argc:%d\n", file_list.uris[file_list.cur_index], argc);
  agmp_set_uri(handle, file_list.uris[file_list.cur_index]);
  agmp_set_license_url(handle, license_url);
  agmp_set_window_size(handle, x, y, w, h);
  agmp_prepare(handle);

  /* play */
  agmp_play (handle);

  gst_print ("I am main function, waitting for quit.\n");
  void *value_ptr = NULL;
  pthread_join(cmdThreadId, &value_ptr);
  /* clean up */
  agmp_exit(handle);

  gst_print ("main function quit\n");
  return 0;
}
