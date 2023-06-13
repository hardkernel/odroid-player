/* GStreamer command line playback testing utility
 *
 * Copyright (C) 2013-2014
 * Copyright (C) 2013
 * Copyright (C) 2015
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

//#include <locale.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "keyboard.h"
#include "porting.h"

typedef int bool;
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
static bool main_quit = FALSE;
unsigned int timer_id;

static bool play_next (AGMP_HANDLE handle);
static bool play_prev (AGMP_HANDLE handle);
static bool agmp_timeout (void* user_data);
static void agmp_message_callback(AGMP_HANDLE handle, AGMP_MESSAGE_TYPE type);
static void keyboard_cb (const char * key_input, void* user_data);

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
static bool agmp_timeout (void* user_data)
{
  AGMP_HANDLE handle = user_data;
  long pos = -1, dur = -1;
  char *status;

  if (main_quit)
  {
    return FALSE;
  }
  dur = agmp_get_duration(handle);
  pos = agmp_get_position(handle);

  if (pos >= 0 && dur > 0) {
    char dstr[32], pstr[32];

    /* FIXME: pretty print in nicer format */
    snprintf (pstr, 32, "%ld:%02ld:%02ld.%03ld", pos/SECOND/3600, pos/SECOND/60%60, pos/SECOND%60, pos%SECOND);
    pstr[12] = '\0';
    snprintf (dstr, 32, "%ld:%02ld:%02ld.%03ld", dur/SECOND/3600, dur/SECOND/60%60, dur/SECOND%60, dur%SECOND);
    dstr[12] = '\0';
    gst_print ("%s / %s\r", pstr, dstr);
  }

  return TRUE;
}

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

static void agmp_message_callback(AGMP_HANDLE handle, AGMP_MESSAGE_TYPE type)
{
  switch (type)
  {
    case AGMP_MESSAGE_BUFFERING:
      gst_print ("message BUFFERING: %d%%.....\r", agmp_get_buffering_percent(handle));
    break;
    case AGMP_MESSAGE_ASYNC_DONE:
    {
      gst_print ("message ASYNC_DONE.....\n");
    }
    break;
    case AGMP_MESSAGE_EOS:
      gst_print ("message EOS.....\n");
      if (!play_next (handle)) {
        gst_print ("reach the filelist end, stop.\n");
        agmp_stop (handle);
        aamp_destroy_timer(timer_id);
        agmp_exit(handle);
        main_quit=TRUE;
      }
    break;
    case AGMP_MESSAGE_ERROR:
      gst_print ("message ERROR.....\n");
      if (!play_next (handle)) {
        gst_print ("reach the filelist end, stop.\n");
        agmp_stop (handle);
        aamp_destroy_timer(timer_id);
        agmp_exit(handle);
        main_quit=TRUE;
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

static void restore_terminal (void)
{
  gst_play_kb_set_key_handler (NULL, NULL);
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

AGMP_PLAY_SPEED play_speed = AGMP_PLAY_SPEED_1;
static void command_cb (const char * input, void* user_data)
{
  double value = 0;
  AGMP_HANDLE handle = (AGMP_HANDLE) user_data;
  char cmd[INPUT_MAX_LEN] = {0};

  //copy cmd and trim
  int copylen = strlen(input) < INPUT_MAX_LEN ? strlen(input) : INPUT_MAX_LEN-1;
  strncpy(cmd, input, copylen);
  gst_print("agmplayer>%s", input);
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
    aamp_destroy_timer(timer_id);
    agmp_exit(handle);
		main_quit=TRUE;
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
  else
  {

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
  bool verbose = FALSE;
  bool print_version = FALSE;
  bool interactive = TRUE;
  bool gapless = FALSE;
  bool shuffle = FALSE;
  char **filenames = NULL;
  char license_url[1024] = {0};
  char *playlist_file = NULL;
  int x=0, y=0, w=0, h=0;
  /*GOptionEntry options[] = {
    {"verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
        N_("Output status information and property notifications"), NULL},
    {"version", 0, 0, G_OPTION_ARG_NONE, &print_version,
        N_("Print version information and exit"), NULL},
    {"shuffle", 0, 0, G_OPTION_ARG_NONE, &shuffle,
        N_("Shuffle playlist"), NULL},
    {"no-interactive", 0, G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE,
          &interactive,
        N_("Disable interactive control via the keyboard"), NULL},
    {"quiet", 'q', 0, G_OPTION_ARG_NONE, &quiet,
        N_("Do not print any output (apart from errors)"), NULL},
    {G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL},
    {"license-url", 0, 0, G_OPTION_ARG_STRING, &license_url, N_("license-url"), NULL},
    {NULL}
  };*/

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

  if (interactive) {
    if (gst_play_kb_set_key_handler (command_cb, handle)) {
      gst_print ("Press 'k' to see a list of keyboard shortcuts.\n");
      atexit (restore_terminal);
    } else {
      gst_print ("Interactive keyboard handling in terminal not available.\n");
    }
  }

  aamp_register_events(handle, agmp_message_callback);
  file_list.cur_index = 0;
  gst_print ("\n uris %s, argc:%d\n", file_list.uris[file_list.cur_index], argc);
  agmp_set_uri(handle, file_list.uris[file_list.cur_index]);
  agmp_set_license_url(handle, license_url);
  agmp_set_window_size(handle, x, y, w, h);
  agmp_prepare(handle);
  collect_media_info(handle);

  timer_id = aamp_create_timer(100, agmp_timeout, handle);
  /* play */
  agmp_play (handle);

  main_quit = FALSE;
  gst_print ("I am main function, waitting for quit.\n");
  while (!main_quit)
  {
	  sleep(1);
  }

  gst_print ("main function quit\n");
  return 0;
}
