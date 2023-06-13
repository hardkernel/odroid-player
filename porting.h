/* GStreamer command line playback testing utility - keyboard handling helpers
 *
 * Copyright (C) 2013
 * Copyright (C) 2013
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
#ifndef __PORTING_INCLUDED__
#define __PORTING_INCLUDED__

#include <stdio.h>

typedef enum
{
  AGMP_STATUS_NULL = 0,
  AGMP_STATUS_PREPARED,
  AGMP_STATUS_PLAYING,
  AGMP_STATUS_PAUSED,
  AGMP_STATUS_STOPED,
} AGMP_SSTATUS;

#define AAMP_SUCCESS 0
#define AAMP_FAILED 1
#define AAMP_NULL_POINTER 2
#define AAMP_INVALID_PARAM 3
#define AAMP_FAILED_IN_THIS_STATE 4


typedef enum
{
  AGMP_PLAY_SPEED_1_4 = 0,
  AGMP_PLAY_SPEED_1_2,
  AGMP_PLAY_SPEED_1,
  AGMP_PLAY_SPEED_2,
  AGMP_PLAY_SPEED_4,
  AGMP_PLAY_SPEED_8,
} AGMP_PLAY_SPEED;

typedef enum
{
  AGMP_MESSAGE_BUFFERING = 0,
  AGMP_MESSAGE_ASYNC_DONE,
  AGMP_MESSAGE_EOS,
  AGMP_MESSAGE_ERROR, //need replay or play another uri
  AGMP_MESSAGE_VIDEO_UNDERFLOW,
  AGMP_MESSAGE_AUDIO_UNDERFLOW,
  AGMP_MESSAGE_FIRST_VFRAME,
  AGMP_MESSAGE_FIRST_AFRAME,
  AGMP_MESSAGE_SEEK_DONE,
  AGMP_MESSAGE_MEDIA_INFO_CHANGED,
} AGMP_MESSAGE_TYPE;

typedef enum
{
  GST_PLAY_TRICK_MODE_NONE = 0,
  GST_PLAY_TRICK_MODE_DEFAULT,
  GST_PLAY_TRICK_MODE_DEFAULT_NO_AUDIO,
  GST_PLAY_TRICK_MODE_KEY_UNITS,
  GST_PLAY_TRICK_MODE_KEY_UNITS_NO_AUDIO,
  GST_PLAY_TRICK_MODE_LAST
} GstPlayTrickMode;

typedef enum
{
  AAMP_TRACK_TYPE_INVALID = 0,
  AAMP_TRACK_TYPE_AUDIO,
  AAMP_TRACK_TYPE_VIDEO,
  AAMP_TRACK_TYPE_SUBTITLE
} AAMPTrackType;

#define INFO_STRING_MAXLEN 100
typedef struct
{
  int track_id;
  char codec[INFO_STRING_MAXLEN];
  char container[INFO_STRING_MAXLEN];
  int width;
  int height;
  int framerate;
} VideoInfo;

typedef struct
{
  int track_id;
  char codec[INFO_STRING_MAXLEN];
  char container[INFO_STRING_MAXLEN];
  int samples;
  int channels;
  unsigned int rate;
} AudioInfo;

typedef struct
{
  int track_id;
  char codec[INFO_STRING_MAXLEN];
  char lang[INFO_STRING_MAXLEN];
} TextInfo;

#define AGMP_HANDLE void*
typedef void (*timeout_callback) (AGMP_HANDLE handle);
typedef void (*message_callback) (AGMP_HANDLE handle, AGMP_MESSAGE_TYPE type);

AGMP_HANDLE agmp_init (void);
int agmp_set_uri(AGMP_HANDLE handle, char* uri); //called before agmp_prepare
int agmp_set_license_url(AGMP_HANDLE handle, char* license_url); //called before agmp_prepare if need license_url
int agmp_set_volume(AGMP_HANDLE handle, double volume);
int agmp_prepare (AGMP_HANDLE handle);
int agmp_play (AGMP_HANDLE handle);
int agmp_stop (AGMP_HANDLE handle);
int agmp_pause (AGMP_HANDLE handle);
int agmp_exit (AGMP_HANDLE handle);
long long agmp_get_duration(AGMP_HANDLE handle);
long long agmp_get_position(AGMP_HANDLE handle);
int agmp_set_speed(AGMP_HANDLE handle, AGMP_PLAY_SPEED rate);
int agmp_seek(AGMP_HANDLE handle, double position);
AGMP_SSTATUS agmp_get_state(AGMP_HANDLE handle);

unsigned int aamp_create_timer(unsigned int interval, timeout_callback callback, AGMP_HANDLE handle);
void aamp_destroy_timer(unsigned int timer_id);
int aamp_register_events(AGMP_HANDLE handle, message_callback callback);
void agmp_deinit (AGMP_HANDLE handle);

int agmp_set_window_size(AGMP_HANDLE handle, int x, int y, int w, int h); //called before agmp_prepare
int aamp_get_media_track_num(AGMP_HANDLE handle, int* pn_video, int* pn_audio, int* pn_text);
int aamp_get_video_track_info(AGMP_HANDLE handle, int trackid, VideoInfo* video_info);
int aamp_get_audio_track_info(AGMP_HANDLE handle, int trackid, AudioInfo* audio_info);
int aamp_get_text_track_info(AGMP_HANDLE handle, int trackid, TextInfo* text_info);
int aamp_set_audio_track(AGMP_HANDLE handle, int trackid);
int agmp_get_buffering_percent(AGMP_HANDLE handle);
#endif /* __PORTING_LAYER_INCLUDED__ */
