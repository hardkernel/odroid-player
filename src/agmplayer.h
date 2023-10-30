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
#ifndef __AGMPLAYER_INCLUDED__
#define __AGMPLAYER_INCLUDED__

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
  AGMP_MESSAGE_STATE_CHANGE,
  AGMP_MESSAGE_AAMP_STATE_CHANGE,//support aamp
  AGMP_MESSAGE_PROGRESS_UPDATE,
} AGMP_MESSAGE_TYPE;

/* log */
typedef enum
{
  LOG_TRACE,
  LOG_DEBUG,
  LOG_INFO,
  LOG_WARN,
  LOG_ERROR,
  LOG_FATAL,
} LOG_LEVEL;

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
typedef void (*message_callback) (AGMP_HANDLE handle, AGMP_MESSAGE_TYPE type, void* userdata);

AGMP_HANDLE agmp_init (void);
int agmp_set_uri(AGMP_HANDLE handle, const char* uri); //called before agmp_prepare
int agmp_set_license_url(AGMP_HANDLE handle, char* license_url); //called before agmp_prepare if need license_url
int agmp_set_volume(AGMP_HANDLE handle, double volume);
double agmp_get_volume(AGMP_HANDLE handle);
int agmp_set_video_mute(AGMP_HANDLE handle, int mute);
int agmp_prepare (AGMP_HANDLE handle);
int agmp_play (AGMP_HANDLE handle);
int agmp_stop (AGMP_HANDLE handle);
int agmp_pause (AGMP_HANDLE handle);
int agmp_exit (AGMP_HANDLE handle);
long long agmp_get_duration(AGMP_HANDLE handle);
long long agmp_get_position(AGMP_HANDLE handle);
int agmp_set_speed(AGMP_HANDLE handle, AGMP_PLAY_SPEED rate);
int agmp_get_speed(AGMP_HANDLE handle);
int agmp_seek(AGMP_HANDLE handle, double position);
AGMP_SSTATUS agmp_get_state(AGMP_HANDLE handle);


int aamp_register_events(AGMP_HANDLE handle, message_callback callback, void* userdata);
void agmp_deinit (AGMP_HANDLE handle);

int agmp_set_window_size(AGMP_HANDLE handle, int x, int y, int w, int h); //called before agmp_prepare
int agmp_get_window_size(AGMP_HANDLE handle, int* x, int* y, int* w, int* h);
int agmp_set_zoom(AGMP_HANDLE handle, int zoom);
int aamp_get_media_track_num(AGMP_HANDLE handle, int* pn_video, int* pn_audio, int* pn_text);
int aamp_get_video_track_info(AGMP_HANDLE handle, int trackid, VideoInfo* video_info);
int aamp_get_audio_track_info(AGMP_HANDLE handle, int trackid, AudioInfo* audio_info);
int aamp_get_text_track_info(AGMP_HANDLE handle, int trackid, TextInfo* text_info);
int aamp_set_audio_track(AGMP_HANDLE handle, int trackid);
int agmp_get_buffering_percent(AGMP_HANDLE handle);
int agmp_set_log_level (LOG_LEVEL level);

/* support aamp */
unsigned int agmp_get_aamp_state(AGMP_HANDLE handle);
int agmp_set_zoom(AGMP_HANDLE handle, int zoom);
int agmp_set_video_mute(AGMP_HANDLE handle, int mute);
#endif /* __AGMPLAYER_INCLUDED__ */
