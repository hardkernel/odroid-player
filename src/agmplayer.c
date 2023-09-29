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


#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <gst/video/video.h>
#include <gst/pbutils/pbutils.h>
#include <gst/tag/tag.h>
#include <gst/math-compat.h>
#include "agmplayer.h"

typedef struct
{
  int x;
  int y;
  int w;
  int h;
} WindowSize;

/* PrivAAMPState is for aamp*/
typedef enum
{
	eSTATE_IDLE,         /**< 0  - Player is idle */
	eSTATE_INITIALIZING, /**< 1  - Player is initializing a particular content */
	eSTATE_INITIALIZED,  /**< 2  - Player has initialized for a content successfully */
	eSTATE_PREPARING,    /**< 3  - Player is loading all associated resources */
	eSTATE_PREPARED,     /**< 4  - Player has loaded all associated resources successfully */
	eSTATE_BUFFERING,    /**< 5  - Player is in buffering state */
	eSTATE_PAUSED,       /**< 6  - Playback is paused */
	eSTATE_SEEKING,      /**< 7  - Seek is in progress */
	eSTATE_PLAYING,      /**< 8  - Playback is in progress */
	eSTATE_STOPPING,     /**< 9  - Player is stopping the playback */
	eSTATE_STOPPED,      /**< 10 - Player has stopped playback successfully */
	eSTATE_COMPLETE,     /**< 11 - Playback completed */
	eSTATE_ERROR,        /**< 12 - Error encountered and playback stopped */
	eSTATE_RELEASED,     /**< 13 - Player has released all resources for playback */
	eSTATE_BLOCKED       /**< 14 - Player has blocked and cant play content*/
} PrivAAMPState;

typedef struct
{
  const gchar *uri;
  gchar *license_url;
  AGMP_SSTATUS status;

  GstElement *playbin;

  GstElement *uridb;
  GstElement *db;
  GstElement *pb;
  GstElement *dmx;
  GstElement *mq;
  GstElement *vdec;
  GstElement *adec;
  GstElement *vsink;
  GstElement *asink;
  GstElement *wlcdmi;

  GstElement *playsink;
  GstElement *abin;
  GstElement *aq;
  GstElement *vbin;
  GstElement *vq;

  /* playbin3 variables */
  gboolean is_playbin3;
  GstStreamCollection *collection;
  gchar *cur_audio_sid;
  gchar *cur_video_sid;
  gchar *cur_text_sid;
  GMutex selection_lock;

  GMainLoop *loop;
  guint bus_watch;
  GThread *play_thread;
  message_callback notify_app;
  WindowSize win_size;
  int percent;
  gboolean async_done;

  /* missing plugin messages */
  GList *missing;

  gboolean buffering;
  gboolean is_live;

  GstState desired_state;       /* as per user interaction, PAUSED or PLAYING */

  gulong deep_notify_id;

  /* configuration */
  gboolean gapless;
  gboolean wait_on_eos;

  GstPlayTrickMode trick_mode;
  gdouble rate;
  double volume;

  /*support aamp*/
  PrivAAMPState aamp_state;
  gboolean video_muted;
  gboolean audio_muted;
  char videoRectangle[32];
  void* userdata;
} GstPlay;

typedef enum
{
  GST_PLAY_TRACK_TYPE_INVALID = 0,
  GST_PLAY_TRACK_TYPE_AUDIO,
  GST_PLAY_TRACK_TYPE_VIDEO,
  GST_PLAY_TRACK_TYPE_SUBTITLE
} GstPlayTrackType;

static gboolean quiet = FALSE;
static gboolean play_bus_msg (GstBus * bus, GstMessage * msg, gpointer data);
//static void play_about_to_finish (GstElement * playbin, gpointer user_data);
static int play_reset (GstPlay * player);
static gboolean play_do_seek (GstPlay * play, gint64 pos, gdouble rate,
    GstPlayTrickMode mode);
static void relative_seek (GstPlay * play, gdouble percent);
static void default_element_added(GstBin *bin, GstElement *element, gpointer user_data);

void aamp_switch_trick_mode (GstPlay * play);
int get_audio_track_num(GstPlay * play);
static void play_track_selection (GstPlay * play, GstPlayTrackType track_type, gint index);
static void play_set_playback_rate (GstPlay * play, gdouble rate);
static int set_pipeline(GstPlay *player, gboolean use_playbin3, const gchar *flags_string, char* audio_sink, char* video_sink);
static int agmp_replay (AGMP_HANDLE handle);
static void set_aamp_state(GstPlay *player, PrivAAMPState state);

/* log */
enum { LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL };

#define log_trace(...) log_log(LOG_TRACE, __func__, __LINE__, __VA_ARGS__)
#define log_debug(...) log_log(LOG_DEBUG, __func__, __LINE__, __VA_ARGS__)
#define log_info(...)  log_log(LOG_INFO,  __func__, __LINE__, __VA_ARGS__)
#define log_warn(...)  log_log(LOG_WARN,  __func__, __LINE__, __VA_ARGS__)
#define log_error(...) log_log(LOG_ERROR, __func__, __LINE__, __VA_ARGS__)
#define log_fatal(...) log_log(LOG_FATAL, __func__, __LINE__, __VA_ARGS__)
#define gst_print(...) log_log(LOG_INFO, __func__, __LINE__, __VA_ARGS__)

static struct {
  void *udata;
  int level;
  int quiet;
} L = {0};

static const char *level_names[] = {
  "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

static void log_log(int level, const char *file, int line, const char *fmt, ...) {
  if (level < L.level) {
    return;
  }
  /* Get current time */
   struct timespec tm;
   long second, usec;

   clock_gettime( CLOCK_MONOTONIC_RAW, &tm );
   second = tm.tv_sec;
   usec = tm.tv_nsec/1000LL;

  /* Log to stderr */
  if (!L.quiet) {
    va_list args;
    printf("[%ld.%06ld]: %-5s %s:%d [AGMPlayer]: ", second, usec, level_names[level], file, line);
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
  }
}

static void default_element_added(GstBin *bin, GstElement *element, gpointer user_data)
{
  GstPlay *play;
  if (NULL == user_data)
  {
    gst_print ("user_data is null.\n");
    return;
  }

  play = (GstPlay *)user_data;

  GST_DEBUG("New element added to %s : %s", GST_ELEMENT_NAME(bin), GST_ELEMENT_NAME(element));
  g_print("New element added to %s : %s\n", GST_ELEMENT_NAME(bin), GST_ELEMENT_NAME(element));

  if (g_strrstr(GST_ELEMENT_NAME(element), "uridecodebin"))
  {
      g_signal_connect(element, "element-added", G_CALLBACK(default_element_added), play);
      play->uridb = element;
  }
  else if (g_strrstr(GST_ELEMENT_NAME(element), "decodebin"))
  {
      g_signal_connect(element, "element-added", G_CALLBACK(default_element_added), play);
      play->db = element;
  }
  else if (g_strrstr(GST_ELEMENT_NAME(element), "parsebin"))
  {
      g_signal_connect(element, "element-added", G_CALLBACK(default_element_added), play);
      play->pb = element;
  }
  else if (g_strrstr(GST_ELEMENT_NAME(element), "vbin"))
  {
      g_signal_connect(element, "element-added", G_CALLBACK(default_element_added), play);
      play->vbin = element;
  }
  else if (g_strrstr(GST_ELEMENT_NAME(element), "abin"))
  {
      g_signal_connect(element, "element-added", G_CALLBACK(default_element_added), play);
      play->abin = element;
  }
  else if (g_strrstr(GST_ELEMENT_NAME(element), "demux"))
  {
      play->dmx = element;
  }
  else if (g_strrstr(GST_ELEMENT_NAME(element), "multiqueue"))
  {
      play->mq = element;
  }
  else if (g_strrstr(GST_ELEMENT_NAME(element), "vqueue"))
  {
      play->vq = element;
  }
  else if (g_strrstr(GST_ELEMENT_NAME(element), "aqueue"))
  {
      play->aq = element;
  }
  else if (g_strrstr(GST_ELEMENT_NAME(element), "sink"))
  {
      if (g_strrstr(GST_ELEMENT_NAME(element), "westeros") || g_strrstr(GST_ELEMENT_NAME(element), "amlvideosink"))
      {
          g_print("\n\n\ndefault_element_added: find vsink:%s\n\n\n", GST_ELEMENT_NAME(element));
          play->vsink = element;
      }
      else if (g_strrstr(GST_ELEMENT_NAME(element), "amlhalasink"))
      {
          play->asink = element;
      }
  }
  else if (g_strrstr(GST_ELEMENT_NAME(element), "dec"))
  {
      if (g_strrstr(GST_ELEMENT_NAME(element), "v4l2"))
      {
          play->vdec = element;
      }
      else if (g_strrstr(GST_ELEMENT_NAME(element), "avdec_"))
      {
          play->adec = element;
      }
  }
  else if (g_strrstr(GST_ELEMENT_NAME(element), "wlcdmi"))
  {
      play->wlcdmi = element;
      if (play->license_url)
      {
        g_object_set (play->wlcdmi, "license-url", play->license_url, NULL);
      }
  }
}

static void callback_to_app(GstPlay *player, AGMP_MESSAGE_TYPE type, void * userdata)
{
  if (NULL == player)
  {
    gst_print ("player is null.\n");
    return;
  }
  if (player->notify_app)
  {
    player->notify_app(player, type, userdata);
  }
}

#define CHECK_POINTER_VALID(p) \
  do { \
    if (NULL == (p)) { \
      gst_print ("pointer is null.\n"); \
      return AAMP_NULL_POINTER; \
    } \
  } while(0);

static void video_underflow(gpointer handle)
{
  if (NULL == handle)
  {
    gst_print ("handle is null.\n");
    return;
  }
  GstPlay *player = handle;
  callback_to_app(player, AGMP_MESSAGE_VIDEO_UNDERFLOW, player->userdata);
}

static void video_first_frame(gpointer handle)
{
  if (NULL == handle)
  {
    gst_print ("handle is null.\n");
    return;
  }
  GstPlay *player = handle;
  callback_to_app(player, AGMP_MESSAGE_FIRST_VFRAME, player->userdata);
}

static void audio_underflow(gpointer handle)
{
  if (NULL == handle)
  {
    gst_print ("handle is null.\n");
    return;
  }
  GstPlay *player = handle;
  callback_to_app(player, AGMP_MESSAGE_AUDIO_UNDERFLOW, player->userdata);
}

static void audio_first_frame(gpointer user_data)
{
  if (NULL == user_data)
  {
    gst_print ("user_data is null.\n");
    return;
  }
  GstPlay *player = user_data;
  callback_to_app(player, AGMP_MESSAGE_FIRST_AFRAME, player->userdata);
}

int agmp_set_uri(AGMP_HANDLE handle, const char* uri)
{
  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;
  player->uri = uri;
  return AAMP_SUCCESS;
}

int agmp_set_license_url(AGMP_HANDLE handle, char* license_url)
{
  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;
  player->license_url = license_url;
  return AAMP_SUCCESS;
}

static gpointer play_run_thread(gpointer data)
{
  if (NULL == data)
  {
    gst_print ("play thread failed\n");
    return NULL;
  }
  GstPlay *player = data;

  CHECK_POINTER_VALID(player);
  gst_print ("play thread enter\n");
  //block here
  g_main_loop_run (player->loop);
  gst_print ("play thread quit\n");

  return NULL;
}

AGMP_HANDLE agmp_init (void)
{
  int argc = 0;
  char **argv = NULL;
  gst_init(&argc, &argv);

  GstPlay *player;

  player = g_new0 (GstPlay, 1);
  if (NULL == player)
  {
    gst_print ("new player failed.\n");
	  return NULL;
  }

  player->uri = NULL;
  player->license_url = NULL;
  player->status = AGMP_STATUS_NULL;

  player->playbin = NULL;
  player->is_playbin3 = FALSE;
  player->asink = NULL;
  player->vsink = NULL;

  g_mutex_init (&player->selection_lock);
  player->deep_notify_id = 0;
  player->loop = NULL;
  player->bus_watch = 0;
  player->notify_app = NULL;
  player->userdata = NULL;

  player->missing = NULL;
  player->buffering = FALSE;
  player->is_live = FALSE;
  player->gapless = FALSE;
  player->wait_on_eos = FALSE;
  player->rate = 1.0;
  player->trick_mode = GST_PLAY_TRICK_MODE_NONE;
  player->win_size.x = 0;
  player->win_size.y = 0;
  player->win_size.w = 0;
  player->win_size.h = 0;
  player->percent = 0;
  player->async_done = FALSE;
  player->aamp_state = eSTATE_IDLE;

  gboolean use_playbin3 = FALSE;
  gchar *flags_string = NULL;
  char* audio_sink = "amlhalasink";
  char* video_sink = "westerossink";

  const gchar *sink_name = g_getenv ("GST_CFG_VIDEO_SINK");
  if ( sink_name )
  {
     if (strstr(sink_name, "westerossink"))
        video_sink = "westerossink";
     else if(strstr(sink_name, "amlvideosink"))
        video_sink = "amlvideosink";
     else if(strstr(sink_name, "clutterautovideosink"))
        video_sink = "clutterautovideosink";
     else
        video_sink = "westerossink";
  }

  GstElement *playbin = NULL;
  if (use_playbin3) {
    playbin = gst_element_factory_make ("playbin3", "playbin");
  } else {
    playbin = gst_element_factory_make ("playbin", "playbin");
  }
  if (playbin == NULL) {
    gst_print ("make playbin failed.\n");
    return NULL;
  }

  player->playbin = playbin;
  g_signal_connect(playbin, "element-added", G_CALLBACK(default_element_added), player);

  if (use_playbin3) {
    player->is_playbin3 = TRUE;
  } else {
    const gchar *env = g_getenv ("USE_PLAYBIN3");
    if (env && g_str_has_prefix (env, "1"))
      player->is_playbin3 = TRUE;
  }

  GstElement *sink = NULL;
  //asink
  if (audio_sink != NULL) {
    if (strchr (audio_sink, ' ') != NULL)
      sink = gst_parse_bin_from_description (audio_sink, TRUE, NULL);
    else
      sink = gst_element_factory_make (audio_sink, NULL);

    if (sink != NULL) {
      g_object_set (player->playbin, "audio-sink", sink, NULL);
      g_object_set (sink, "wait-video", TRUE, NULL);
      g_object_set (sink, "a-wait-timeout", 600, NULL);
      g_signal_connect_swapped (sink, "underrun-callback", G_CALLBACK (audio_underflow), player);
      //g_signal_connect_swapped (sink, "first-audio-frame-callback", G_CALLBACK(audio_first_frame), player);
    }
    else
      g_warning ("Couldn't create specified audio sink '%s'", audio_sink);
    player->asink = sink;
  }

  //vsink
  if (video_sink != NULL) {
    if (strchr (video_sink, ' ') != NULL)
      sink = gst_parse_bin_from_description (video_sink, TRUE, NULL);
    else
      sink = gst_element_factory_make (video_sink, NULL);

    if (sink != NULL) {
      g_object_set (player->playbin, "video-sink", sink, NULL);
      player->vsink = sink;
      //g_object_set (player->vsink, "stop-keep-frame", TRUE, NULL);
      g_signal_connect_swapped (player->vsink, "buffer-underflow-callback", G_CALLBACK (video_underflow), player);
      g_signal_connect_swapped (player->vsink, "first-video-frame-callback", G_CALLBACK (video_first_frame), player);
    }
    else
      g_warning ("Couldn't create specified video sink '%s'", video_sink);
  }

  if (flags_string != NULL) {
    GParamSpec *pspec;
    GValue val = { 0, };

    pspec =
        g_object_class_find_property (G_OBJECT_GET_CLASS (playbin), "flags");
    g_value_init (&val, pspec->value_type);
    if (gst_value_deserialize (&val, flags_string))
      g_object_set_property (G_OBJECT (player->playbin), "flags", &val);
    else
      gst_printerr ("Couldn't convert '%s' to playbin flags!\n", flags_string);
    g_value_unset (&val);
  }

  /*if (verbose) {
    player->deep_notify_id =
        gst_element_add_property_deep_notify_watch (player->playbin, NULL, TRUE);
  }*/

  player->loop = g_main_loop_new (NULL, FALSE);
  player->bus_watch = gst_bus_add_watch (GST_ELEMENT_BUS (player->playbin), play_bus_msg, player);

    //create play thread
  player->play_thread = g_thread_new ("video play run thread", play_run_thread, player);
  if (!player->play_thread) {
      gst_print ("fail to create play thread");
      return NULL;
  }

  L.level = LOG_DEBUG;
  return (AGMP_HANDLE)player;
}

int agmp_prepare (AGMP_HANDLE handle)
{
  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;

  gst_print ("prepare stream enter.\n");
  set_aamp_state(player, eSTATE_PREPARING);
  if (AGMP_STATUS_PREPARED == player->status)
  {
	  gst_print ("already playing: %d.", player->status);
    return AAMP_SUCCESS;
  }

  if (player->status != AGMP_STATUS_NULL && player->status != AGMP_STATUS_STOPED)
  {
    gst_print ("can't be called in this state: %d.", player->status);
    return AAMP_FAILED_IN_THIS_STATE;
  }

  play_reset (player);
  g_object_set (player->playbin, "uri", player->uri, NULL);
  gboolean ret = TRUE;
  player->async_done = FALSE;
  switch (gst_element_set_state (player->playbin, GST_STATE_PAUSED)) {
  case GST_STATE_CHANGE_FAILURE:
    gst_print ("Pipeline state change fail.\n");
    /* ignore, we should get an error message posted on the bus */
    ret = FALSE;
    break;
  case GST_STATE_CHANGE_NO_PREROLL:
    gst_print ("Pipeline is live.\n");
    player->is_live = TRUE;
    break;
  case GST_STATE_CHANGE_ASYNC:
    gst_print ("Prerolling...\r");
    break;
  default:
    gst_print ("Pipeline to paused.\n");
    break;
  }

  if (!ret)
    return AAMP_FAILED;

  //timeout or async-done
  int second = 50;
  while (!player->async_done && second > 0)
  {
    usleep(100000);
    second--;
  }

  if (!second) {
    gst_print ("prepare stream timeout.\n");
    return AAMP_FAILED;
  }
  player->async_done = FALSE;
  player->status = AGMP_STATUS_PREPARED;
  gst_print ("prepare stream over.\n");
  set_aamp_state(player, eSTATE_PREPARED);
  return AAMP_SUCCESS;
}

int agmp_play (AGMP_HANDLE handle)
{
  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;

  gst_print ("play.\n");
  if (AGMP_STATUS_PLAYING == player->status)
  {
	  gst_print ("already playing: %d.", player->status);
    return AAMP_SUCCESS;
  }

  if (player->status != AGMP_STATUS_PREPARED && player->status != AGMP_STATUS_PAUSED && player->status != AGMP_STATUS_STOPED)
  {
    gst_print ("can't be called in this state: %d.", player->status);
    return AAMP_FAILED_IN_THIS_STATE;
  }

  player->desired_state = GST_STATE_PLAYING;
  gst_element_set_state (player->playbin, GST_STATE_PLAYING);
  player->status = AGMP_STATUS_PLAYING;
  set_aamp_state(player, eSTATE_PLAYING);
  return AAMP_SUCCESS;
}

int agmp_pause (AGMP_HANDLE handle)
{
  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;

  if (AGMP_STATUS_PAUSED == player->status)
  {
	gst_print ("already paused: %d.", player->status);
    return AAMP_SUCCESS;
  }

  if (player->status != AGMP_STATUS_PLAYING)
  {
    gst_print ("can't be called in this state: %d.", player->status);
    return AAMP_FAILED_IN_THIS_STATE;
  }

  if (player->buffering) {
    gst_print ("I am buffering, no need pause\n");
	  return AAMP_SUCCESS;
  }

  player->desired_state = GST_STATE_PAUSED;
  gst_element_set_state (player->playbin, GST_STATE_PAUSED);
  player->status = AGMP_STATUS_PAUSED;
  set_aamp_state(player, eSTATE_PAUSED);

  return AAMP_SUCCESS;
}

int agmp_stop (AGMP_HANDLE handle)
{
  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;

  if (AGMP_STATUS_STOPED == player->status)
  {
	  gst_print ("already stoped: %d.", player->status);
    return AAMP_SUCCESS;
  }

  if (player->status != AGMP_STATUS_PREPARED && player->status != AGMP_STATUS_PLAYING && player->status != AGMP_STATUS_PAUSED)
  {
    gst_print ("can't be called in this state: %d.", player->status);
    return AAMP_FAILED_IN_THIS_STATE;
  }
  set_aamp_state(player, eSTATE_STOPPING);
  gst_element_set_state (player->playbin, GST_STATE_READY);

  // wait state change
  usleep(1000000);
  set_aamp_state(player, eSTATE_STOPPED);
  player->status = AGMP_STATUS_STOPED;
}

void quit_thread(GstPlay* player)
{
  if (player->play_thread) {
    gst_print ("\njoin thread\n");
    g_thread_join (player->play_thread);
    player->play_thread = NULL;
  }
}

void quit_loop(GstPlay* player)
{
	g_main_loop_quit (player->loop);
}

int agmp_exit (AGMP_HANDLE handle)
{
  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;
  set_aamp_state(player, eSTATE_RELEASED);
  gst_print ("exit enter.\n");
  quit_loop(player);
  agmp_deinit(handle);
  quit_thread(player);
  g_free (player);
  player = NULL;
  //gst_deinit();
  gst_print ("exit over.\n");
  return AAMP_SUCCESS;
}

void agmp_deinit (AGMP_HANDLE handle)
{
  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;

  gst_print ("\nrelease\n");


  /* No need to see all those pad caps going to NULL etc., it's just noise */
  if (player->deep_notify_id != 0)
    g_signal_handler_disconnect (player->playbin, player->deep_notify_id);

  play_reset (player);

  gst_element_set_state (player->playbin, GST_STATE_NULL);
  gst_object_unref (player->playbin);

  g_source_remove (player->bus_watch);

  g_main_loop_unref (player->loop);

  //g_strfreev (player->uri);

  if (player->collection)
    gst_object_unref (player->collection);
  g_free (player->cur_audio_sid);
  g_free (player->cur_video_sid);
  g_free (player->cur_text_sid);

  g_mutex_clear (&player->selection_lock);

}

AGMP_SSTATUS agmp_get_state(AGMP_HANDLE handle)
{
  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;
  return player->status;
}

unsigned int agmp_get_aamp_state(AGMP_HANDLE handle)
{
  if (NULL == handle) {
    return eSTATE_IDLE;
  }
  GstPlay* player = (GstPlay*)handle;
  return player->aamp_state;
}

static void set_aamp_state(GstPlay *player, PrivAAMPState state)
{
  if (NULL == player) {
    return;
  }

  if (player->aamp_state != state) {
    player->aamp_state = state;
    //notify aamp
    callback_to_app(player, AGMP_MESSAGE_AAMP_STATE_CHANGE, player->userdata);
  }
}

long long agmp_get_position(AGMP_HANDLE handle)
{
  if (NULL == handle)
  {
    return -1;
  }
  GstPlay* player = (GstPlay*)handle;

  long long  pos = -1;
  if (player->buffering)
    return pos;
  gst_element_query_position (player->playbin, GST_FORMAT_TIME, &pos);
  return pos;
}

long long agmp_get_duration(AGMP_HANDLE handle)
{
  if (NULL == handle)
  {
    return -1;
  }
  GstPlay* player = (GstPlay*)handle;

  long long  dur = -1;
  if (player->buffering)
    return dur;
  gst_element_query_duration (player->playbin, GST_FORMAT_TIME, &dur);
  return dur;
}

int agmp_set_speed(AGMP_HANDLE handle, AGMP_PLAY_SPEED rate)
{
  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;

  if (player->status != AGMP_STATUS_PLAYING)
  {
    gst_print ("can't be called in this state: %d.", player->status);
    return AAMP_FAILED_IN_THIS_STATE;
  }

  double rate_level[] = {0.125, 0.25, 0.5, 1, 2, 4, 8};
  if (rate > sizeof(rate_level)/sizeof(rate_level[0])) {
    gst_print ("rate out of range, %d.\n", rate);
	  return AAMP_INVALID_PARAM;
  }

  double new_rate = rate_level[rate];

  if (new_rate != player->rate)
  {
    player->rate = new_rate;
    gst_print("set rate to %lf\n", player->rate);
    play_set_playback_rate (player, player->rate);
  }
  else
  {
    gst_print("no need to set rate %lf\n", player->rate);
  }
  return AAMP_SUCCESS;
}

int agmp_get_speed(AGMP_HANDLE handle)
{
  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;
  return player->rate;
}

/* reset for new file/stream */
static int play_reset (GstPlay * player)
{
  CHECK_POINTER_VALID(player);

  g_list_foreach (player->missing, (GFunc) gst_message_unref, NULL);
  player->missing = NULL;

  player->buffering = FALSE;
  player->is_live = FALSE;
  return AAMP_SUCCESS;
}


/* returns TRUE if something was installed and we should restart playback */
static gboolean
play_install_missing_plugins (GstPlay * play)
{
  /* FIXME: implement: try to install any missing plugins we haven't
   * tried to install before */
  return FALSE;
}

int agmp_replay (AGMP_HANDLE handle)
{
  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;

  gst_element_set_state (player->playbin, GST_STATE_READY);
  play_reset (player);

  g_object_set (player->playbin, "uri", player->uri, NULL);
  switch (gst_element_set_state (player->playbin, GST_STATE_PAUSED)) {
    case GST_STATE_CHANGE_FAILURE:
      /* ignore, we should get an error message posted on the bus */
      break;
    case GST_STATE_CHANGE_NO_PREROLL:
      gst_print ("Pipeline is live.\n");
      player->is_live = TRUE;
      break;
    case GST_STATE_CHANGE_ASYNC:
      gst_print ("Prerolling...\r");
      break;
    default:
      break;
  }

  return agmp_play(player);
}

int aamp_set_audio_track(AGMP_HANDLE handle, int trackid)
{
  play_track_selection (handle, GST_PLAY_TRACK_TYPE_AUDIO, (gint)trackid);
}

int set_video_track(AGMP_HANDLE handle, int trackid)
{
  play_track_selection (handle, GST_PLAY_TRACK_TYPE_AUDIO, (gint)trackid);
}

int set_subtitle_track(AGMP_HANDLE handle, int trackid)
{
  play_track_selection (handle, GST_PLAY_TRACK_TYPE_AUDIO, (gint)trackid);
}

int get_audio_track_num(GstPlay * player)
{
  CHECK_POINTER_VALID(player);
  /* playbin3 variables */
  gint nb_audio = 0, nb_video = 0, nb_text = 0;

  g_mutex_lock (&player->selection_lock);
  if (player->is_playbin3) {
    if (!player->collection) {
      gst_print ("No stream-collection\n");
      g_mutex_unlock (&player->selection_lock);
      return;
    }

    /* Check the total number of streams of each type */
    guint len = gst_stream_collection_get_size (player->collection);
    for (guint i = 0; i < len; i++) {
      GstStream *stream =
          gst_stream_collection_get_stream (player->collection, i);
      if (stream) {
        GstStreamType type = gst_stream_get_stream_type (stream);

        if (type & GST_STREAM_TYPE_AUDIO) {
          nb_audio++;
        } else if (type & GST_STREAM_TYPE_VIDEO) {
          nb_video++;
        } else if (type & GST_STREAM_TYPE_TEXT) {
          nb_text++;
        } else {
          gst_print ("Unknown stream type\n");
        }
      }
    }
  }
  else
  {
    gint cur=0;
    guint cur_flags;
    g_object_get (player->playbin, "current-audio", &cur, "n-audio", &nb_audio, "flags", &cur_flags, NULL);
    g_object_get (player->playbin, "current-video", &cur, "n-video", &nb_video, "flags", &cur_flags, NULL);
    g_object_get (player->playbin, "current-text", &cur, "n-text", &nb_text, "flags", &cur_flags, NULL);
  }
  g_mutex_unlock (&player->selection_lock);
  gst_print (
      "audio track number:%d\n" \
      "video track number:%d\n" \
      "subtitle track number:%d\n",
      nb_audio, nb_video, nb_text
  );
  return nb_audio;
}

int agmp_seek(AGMP_HANDLE handle, double position)
{
  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;
  GstQuery *query;
  gboolean seekable = FALSE;

  gst_print("seek to %lf\n", position);
  query = gst_query_new_seeking (GST_FORMAT_TIME);
  if (!gst_element_query (player->playbin, query)) {
    gst_query_unref (query);
    goto seek_failed;
  }

  gint64 dur = -1;
  gst_query_parse_seeking (query, NULL, &seekable, NULL, &dur);
  gst_query_unref (query);

  if (!seekable || dur <= 0)
    goto seek_failed;

  gint64 pos = GST_SECOND * position;

  if (pos > dur) {
    gst_print ("\n%s\n", "Reached end of play list.");
    agmp_stop(player);
  } else {
    if (pos < 0)
      pos = 0;
    play_do_seek (player, pos, player->rate, player->trick_mode);
  }

  return AAMP_SUCCESS;

seek_failed:
  gst_print ("Could not seek\n");
  return AAMP_FAILED;
}

/*int enable_keep_last_frame(GstPlay * player, gboolean enable)
{
  player->wait_on_eos = enable;
}*/

int agmp_get_buffering_percent(AGMP_HANDLE handle)
{
  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;
  return player->percent;
}

static gboolean play_bus_msg (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstPlay *player = user_data;

  if (player == NULL) {
    gst_message_ref (msg);
    return TRUE;
  }

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ASYNC_DONE:

      /* dump graph on preroll */
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (player->playbin),
          GST_DEBUG_GRAPH_SHOW_ALL, "agmplayer.async-done");

      gst_print ("Prerolled.\r");
      if (player->missing != NULL && play_install_missing_plugins (player)) {
        gst_print ("New plugins installed, trying again...\n");
        replay (player);
      }
      player->async_done = TRUE;
      //notify app
      callback_to_app(player, AGMP_MESSAGE_ASYNC_DONE, player->userdata);
      break;
    case GST_MESSAGE_BUFFERING:{
      gint percent;

      gst_message_parse_buffering (msg, &percent);

#if 0
      if (!player->buffering)
        gst_print ("\n");

      gst_print ("%s %d%%  \r", "Buffering...", percent);

      if (percent == 100) {
        // a 100% message means buffering is done
        if (player->buffering) {
          player->buffering = FALSE;
          // no state management needed for live pipelines
          if (!player->is_live)
            gst_element_set_state (player->playbin, player->desired_state);
        }
      } else {
        // buffering...
        if (!player->buffering) {
          if (!player->is_live)
            gst_element_set_state (player->playbin, GST_STATE_PAUSED);
          player->buffering = TRUE;
        }
      }
#endif

      //notify app
      player->percent = percent;
      callback_to_app(player, AGMP_MESSAGE_BUFFERING, player->userdata);
      break;
    }
    case GST_MESSAGE_CLOCK_LOST:{
      gst_print ("Clock lost, selecting a new one\n");
      gst_element_set_state (player->playbin, GST_STATE_PAUSED);
      gst_element_set_state (player->playbin, GST_STATE_PLAYING);
      break;
    }
    case GST_MESSAGE_LATENCY:
      gst_print ("Redistribute latency...\n");
      gst_bin_recalculate_latency (GST_BIN (player->playbin));
      break;
    case GST_MESSAGE_REQUEST_STATE:{
      GstState state;
      gchar *name;

      name = gst_object_get_path_string (GST_MESSAGE_SRC (msg));

      gst_message_parse_request_state (msg, &state);

      gst_print ("Setting state to %s as requested by %s...\n",
          gst_element_state_get_name (state), name);

      gst_element_set_state (player->playbin, state);
      g_free (name);
      break;
    }
    case GST_MESSAGE_EOS:
      // print final position at end
      //play_timeout (player);
      //gst_print ("\n");
      // and switch to next item in list
      /*if (!player->wait_on_eos )
      {
          gst_print ("%s\n", "Reached end of play list.");
          agmp_stop (player);
        }*/

      //notify app
      callback_to_app(player, AGMP_MESSAGE_EOS, player->userdata);
      break;
    case GST_MESSAGE_WARNING:{
      GError *err;
      gchar *dbg = NULL;

      // dump graph on warning
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (player->playbin),
          GST_DEBUG_GRAPH_SHOW_ALL, "agmplayer.warning");

      gst_message_parse_warning (msg, &err, &dbg);
      gst_printerr ("WARNING %s\n", err->message);
      if (dbg != NULL)
        gst_printerr ("WARNING debug information: %s\n", dbg);
      g_clear_error (&err);
      g_free (dbg);

      break;
    }
    case GST_MESSAGE_ERROR:{
      GError *err;
      gchar *dbg;

      // dump graph on error
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (player->playbin),
          GST_DEBUG_GRAPH_SHOW_ALL, "agmplayer.error");

      gst_message_parse_error (msg, &err, &dbg);
      gst_printerr ("ERROR %s for %s\n", err->message,
          player->uri);
      if (dbg != NULL)
        gst_printerr ("ERROR debug information: %s\n", dbg);
      g_clear_error (&err);
      g_free (dbg);

      // flush any other error messages from the bus and clean up
      gst_element_set_state (player->playbin, GST_STATE_NULL);

      if (player->missing != NULL && play_install_missing_plugins (player)) {
        gst_print ("New plugins installed, trying again...\n");
        agmp_replay (player);
        break;
      }
      // try next item in list then
      //if (!agmp_replay (player)) {
        //gst_print ("%s\n", "Reached end of play list.");
        //g_main_loop_quit (player->loop);
      //}
      //notify app
      callback_to_app(player, AGMP_MESSAGE_ERROR, player->userdata);
      break;
    }
    case GST_MESSAGE_ELEMENT:
    {
      GstNavigationMessageType mtype = gst_navigation_message_get_type (msg);
      if (mtype == GST_NAVIGATION_MESSAGE_EVENT) {
        GstEvent *ev = NULL;

        if (gst_navigation_message_parse_event (msg, &ev)) {
          GstNavigationEventType e_type = gst_navigation_event_get_type (ev);
          switch (e_type) {
            case GST_NAVIGATION_EVENT_KEY_PRESS:
            {
              const gchar *key;
              if (gst_navigation_event_parse_key_event (ev, &key)) {
                GST_INFO ("Key press: %s", key);
              }
              break;
            }
            case GST_NAVIGATION_EVENT_MOUSE_BUTTON_PRESS:
            {
              gint button;
              if (gst_navigation_event_parse_mouse_button_event (ev, &button,
                      NULL, NULL)) {
                if (button == 4) {
                  // wheel up
                  //relative_seek (player, +0.08);
                } else if (button == 5) {
                  // wheel down
                  //relative_seek (player, -0.01);
                }
              }
              break;
            }
            default:
              break;
          }
        }
        if (ev)
          gst_event_unref (ev);
      }
      break;
    }
    case GST_MESSAGE_PROPERTY_NOTIFY:{
      const GValue *val;
      const gchar *name;
      GstObject *obj;
      gchar *val_str = NULL;
      gchar *obj_name;

      gst_message_parse_property_notify (msg, &obj, &name, &val);

      obj_name = gst_object_get_path_string (GST_OBJECT (obj));
      if (val != NULL) {
        if (G_VALUE_HOLDS_STRING (val))
          val_str = g_value_dup_string (val);
        else if (G_VALUE_TYPE (val) == GST_TYPE_CAPS)
          val_str = gst_caps_to_string (g_value_get_boxed (val));
        else if (G_VALUE_TYPE (val) == GST_TYPE_TAG_LIST)
          val_str = gst_tag_list_to_string (g_value_get_boxed (val));
        else
          val_str = gst_value_serialize (val);
      } else {
        val_str = g_strdup ("(no value)");
      }

      gst_print ("%s: %s = %s\n", obj_name, name, val_str);
      g_free (obj_name);
      g_free (val_str);
      break;
    }
    case GST_MESSAGE_STREAM_COLLECTION:
    {
      GstStreamCollection *collection = NULL;
      gst_message_parse_stream_collection (msg, &collection);

      if (collection) {
        g_mutex_lock (&player->selection_lock);
        gst_object_replace ((GstObject **) & player->collection,
            (GstObject *) collection);
        g_mutex_unlock (&player->selection_lock);
      }
      gst_print ("stream collect done.\n");
      break;
    }
    case GST_MESSAGE_STREAMS_SELECTED:
    {
      GstStreamCollection *collection = NULL;
      guint i, len;

      gst_message_parse_streams_selected (msg, &collection);
      if (collection) {
        g_mutex_lock (&player->selection_lock);
        gst_object_replace ((GstObject **) & player->collection,
            (GstObject *) collection);

        //Free all last stream-ids
        g_free (player->cur_audio_sid);
        g_free (player->cur_video_sid);
        g_free (player->cur_text_sid);
        player->cur_audio_sid = NULL;
        player->cur_video_sid = NULL;
        player->cur_text_sid = NULL;

        len = gst_message_streams_selected_get_size (msg);
        for (i = 0; i < len; i++) {
          GstStream *stream = gst_message_streams_selected_get_stream (msg, i);
          if (stream) {
            GstStreamType type = gst_stream_get_stream_type (stream);
            const gchar *stream_id = gst_stream_get_stream_id (stream);

            if (type & GST_STREAM_TYPE_AUDIO) {
              player->cur_audio_sid = g_strdup (stream_id);
            } else if (type & GST_STREAM_TYPE_VIDEO) {
              player->cur_video_sid = g_strdup (stream_id);
            } else if (type & GST_STREAM_TYPE_TEXT) {
              player->cur_text_sid = g_strdup (stream_id);
            } else {
              gst_print ("Unknown stream type with stream-id %s", stream_id);
            }
            gst_object_unref (stream);
          }
        }

        gst_object_unref (collection);
        g_mutex_unlock (&player->selection_lock);
      }
      break;
    }
    case GST_MESSAGE_STATE_CHANGED:
    {
      GstState state;
      if (GST_MESSAGE_SRC (msg) == GST_OBJECT (player->playbin)) {
        gst_message_parse_state_changed (msg, NULL, &state, NULL);

        if (state == GST_STATE_VOID_PENDING) {
          gst_print ("bus message status change to pending, %p\n", player->playbin);
          GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (player->playbin),
              GST_DEBUG_GRAPH_SHOW_ALL, "agmplayer.pending");
        }
        else if (state == GST_STATE_NULL) {
          gst_print ("bus message status change to null, %p\n", player->playbin);
        }
        else if (state == GST_STATE_READY) {
          gst_print ("bus message status change to ready, %p\n", player->playbin);
          GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (player->playbin),
              GST_DEBUG_GRAPH_SHOW_ALL, "agmplayer.ready");
        }
        else if (state == GST_STATE_PAUSED) {
          gst_print ("bus message status change to paused, %p\n", player->playbin);
          GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (player->playbin),
              GST_DEBUG_GRAPH_SHOW_ALL, "agmplayer.paused");
        }
        else if (state == GST_STATE_PLAYING) {
          gst_print ("bus message status change to playing, %p\n", player->playbin);
          GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (player->playbin),
              GST_DEBUG_GRAPH_SHOW_ALL, "agmplayer.playing");
        }
        callback_to_app(player, AGMP_MESSAGE_STATE_CHANGE, player->userdata);
      }
      break;
    }
    default:
      /*if (gst_is_missing_plugin_message (msg)) {
        gchar *desc;

        desc = gst_missing_plugin_message_get_description (msg);
        gst_print ("Missing plugin: %s\n", desc);
        g_free (desc);
        player->missing = g_list_append (player->missing, gst_message_ref (msg));
      }*/
      break;
  }
  gst_message_ref (msg);

  return TRUE;
}

int agmp_set_volume(AGMP_HANDLE handle, double volume)
{
  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;

  if (volume > 215)
  {
	volume = 215;
	gst_print("volume is out of range, set max volume[%lf]\n", volume);
  }

  if (volume < 0)
  {
	volume = 0;
	gst_print("volume is out of range, set min volume[%lf]\n", volume);
  }

  volume = ((int)(volume+0.5))/100.0;
  //gst_stream_volume_set_volume (GST_STREAM_VOLUME (player->playbin),
  //GST_STREAM_VOLUME_FORMAT_CUBIC, player->volume );
  if (!player->asink) {
    gst_print ("set volume failed, asink is null.\n");
    return AAMP_FAILED;
  }
  player->volume = volume;
  g_object_set(player->asink, "stream-volume", player->volume, NULL);
  gst_print ("set volume: %.0f%%\n", player->volume  * 100);
  return AAMP_SUCCESS;
}

double agmp_get_volume(AGMP_HANDLE handle)
{
  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;
  return player->volume * 100;
}

int agmp_set_video_mute(AGMP_HANDLE handle, int mute)
{
  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;
  player->video_muted = mute;
  if (!player->vsink) {
    gst_print ("set video mute failed, vsink is null.\n");
    return AAMP_FAILED;
  }
  g_object_set(player->vsink, "mute", player->video_muted, NULL);
  return AAMP_SUCCESS;
}

/*static gchar * play_uri_get_display_name (GstPlay * play, const gchar * uri)
{
  gchar *loc;

  if (gst_uri_has_protocol (uri, "file")) {
    loc = g_filename_from_uri (uri, NULL, NULL);
  } else if (gst_uri_has_protocol (uri, "pushfile")) {
    loc = g_filename_from_uri (uri + 4, NULL, NULL);
  } else {
    loc = g_strdup (uri);
  }

  // Maybe additionally use glib's filename to display name function
  return loc;
}

static void play_about_to_finish (GstElement * playbin, gpointer user_data)
{
  GstPlay *play = user_data;
  const gchar *next_uri;
  gchar *loc;
  guint next_idx;

  if (!play->gapless)
    return;

  next_idx = play->cur_idx + 1;
  if (next_idx >= play->num_uris)
    return;

  next_uri = play->uri;
  loc = play_uri_get_display_name (play, next_uri);
  gst_print ("About to finish, preparing next title: %s", loc);
  gst_print ("\n");
  g_free (loc);

  g_object_set (play->playbin, "uri", next_uri, NULL);
  play->cur_idx = next_idx;
}*/

static gboolean
play_set_rate_and_trick_mode (GstPlay * play, gdouble rate,
    GstPlayTrickMode mode)
{
  gint64 pos = -1;

  g_return_val_if_fail (rate != 0, FALSE);

  if (!gst_element_query_position (play->playbin, GST_FORMAT_TIME, &pos))
    return FALSE;

  return play_do_seek (play, pos, rate, mode);
}

static gboolean
play_do_seek (GstPlay * play, gint64 pos, gdouble rate, GstPlayTrickMode mode)
{
  GstSeekFlags seek_flags;
  GstQuery *query;
  GstEvent *seek;
  gboolean seekable = FALSE;

  query = gst_query_new_seeking (GST_FORMAT_TIME);
  if (!gst_element_query (play->playbin, query)) {
    gst_query_unref (query);
    return FALSE;
  }

  gst_query_parse_seeking (query, NULL, &seekable, NULL, NULL);
  gst_query_unref (query);

  if (!seekable)
    return FALSE;

  seek_flags = GST_SEEK_FLAG_FLUSH;

  switch (mode) {
    case GST_PLAY_TRICK_MODE_DEFAULT:
      seek_flags |= GST_SEEK_FLAG_TRICKMODE;
      break;
    case GST_PLAY_TRICK_MODE_DEFAULT_NO_AUDIO:
      seek_flags |= GST_SEEK_FLAG_TRICKMODE | GST_SEEK_FLAG_TRICKMODE_NO_AUDIO;
      break;
    case GST_PLAY_TRICK_MODE_KEY_UNITS:
      seek_flags |= GST_SEEK_FLAG_TRICKMODE_KEY_UNITS;
      break;
    case GST_PLAY_TRICK_MODE_KEY_UNITS_NO_AUDIO:
      seek_flags |=
          GST_SEEK_FLAG_TRICKMODE_KEY_UNITS | GST_SEEK_FLAG_TRICKMODE_NO_AUDIO;
      break;
    case GST_PLAY_TRICK_MODE_NONE:
    default:
      break;
  }

  if (rate >= 0)
    seek = gst_event_new_seek (rate, GST_FORMAT_TIME,
        seek_flags | GST_SEEK_FLAG_ACCURATE,
        /* start */ GST_SEEK_TYPE_SET, pos,
        /* stop */ GST_SEEK_TYPE_SET, GST_CLOCK_TIME_NONE);
  else
    seek = gst_event_new_seek (rate, GST_FORMAT_TIME,
        seek_flags | GST_SEEK_FLAG_ACCURATE,
        /* start */ GST_SEEK_TYPE_SET, 0,
        /* stop */ GST_SEEK_TYPE_SET, pos);

  if (!gst_element_send_event (play->playbin, seek))
    return FALSE;

  play->rate = rate;
  play->trick_mode = mode;
  return TRUE;
}

static void play_set_playback_rate (GstPlay * play, gdouble rate)
{
  if (play_set_rate_and_trick_mode (play, rate, play->trick_mode)) {
    gst_print ("Playback rate: %.2f", rate);
    gst_print ("                               \n");
  } else {
    gst_print ("\n");
    gst_print ("Could not change playback rate to %.2f", rate);
    gst_print (".\n");
  }
}

static const gchar *trick_mode_get_description (GstPlayTrickMode mode)
{
  switch (mode) {
    case GST_PLAY_TRICK_MODE_NONE:
      return "normal playback, trick modes disabled";
    case GST_PLAY_TRICK_MODE_DEFAULT:
      return "trick mode: default";
    case GST_PLAY_TRICK_MODE_DEFAULT_NO_AUDIO:
      return "trick mode: default, no audio";
    case GST_PLAY_TRICK_MODE_KEY_UNITS:
      return "trick mode: key frames only";
    case GST_PLAY_TRICK_MODE_KEY_UNITS_NO_AUDIO:
      return "trick mode: key frames only, no audio";
    default:
      break;
  }
  return "unknown trick mode";
}

void aamp_switch_trick_mode (GstPlay * play)
{
  GstPlayTrickMode new_mode = ++play->trick_mode;
  const gchar *mode_desc;

  if (new_mode == GST_PLAY_TRICK_MODE_LAST)
    new_mode = GST_PLAY_TRICK_MODE_NONE;

  mode_desc = trick_mode_get_description (new_mode);

  if (play_set_rate_and_trick_mode (play, play->rate, new_mode)) {
    gst_print ("Rate: %.2f (%s)                      \n", play->rate,
        mode_desc);
  } else {
    gst_print ("\nCould not change trick mode to %s.\n", mode_desc);
  }
}

static GstStream *
play_get_nth_stream_in_collection (GstPlay * play, guint index,
    GstPlayTrackType track_type)
{
  guint len, i, n_streams = 0;
  GstStreamType target_type;

  switch (track_type) {
    case GST_PLAY_TRACK_TYPE_AUDIO:
      target_type = GST_STREAM_TYPE_AUDIO;
      break;
    case GST_PLAY_TRACK_TYPE_VIDEO:
      target_type = GST_STREAM_TYPE_VIDEO;
      break;
    case GST_PLAY_TRACK_TYPE_SUBTITLE:
      target_type = GST_STREAM_TYPE_TEXT;
      break;
    default:
      return NULL;
  }

  len = gst_stream_collection_get_size (play->collection);

  for (i = 0; i < len; i++) {
    GstStream *stream = gst_stream_collection_get_stream (play->collection, i);
    GstStreamType type = gst_stream_get_stream_type (stream);

    if (type & target_type) {
      if (index == n_streams)
        return stream;

      n_streams++;
    }
  }

  return NULL;
}

static void play_track_selection (GstPlay * play, GstPlayTrackType track_type, gint index)
{
  const gchar *prop_cur, *prop_n, *prop_get, *name;
  gint n = -1;
  guint flag, cur_flags;

  CHECK_POINTER_VALID(play);
  /* playbin3 variables */
  GList *selected_streams = NULL;
  gint cur_audio_idx = -1, cur_video_idx = -1, cur_text_idx = -1;
  gint nb_audio = 0, nb_video = 0, nb_text = 0;
  guint len, i;

  g_mutex_lock (&play->selection_lock);
  if (play->is_playbin3) {
    if (!play->collection) {
      gst_print ("No stream-collection\n");
      g_mutex_unlock (&play->selection_lock);
      return;
    }

    /* Check the total number of streams of each type */
    len = gst_stream_collection_get_size (play->collection);
    for (i = 0; i < len; i++) {
      GstStream *stream =
          gst_stream_collection_get_stream (play->collection, i);
      if (stream) {
        GstStreamType type = gst_stream_get_stream_type (stream);

        if (type & GST_STREAM_TYPE_AUDIO) {
          nb_audio++;
        } else if (type & GST_STREAM_TYPE_VIDEO) {
          nb_video++;
        } else if (type & GST_STREAM_TYPE_TEXT) {
          nb_text++;
        } else {
          gst_print ("Unknown stream type\n");
        }
      }
    }
  }

  switch (track_type) {
    case GST_PLAY_TRACK_TYPE_AUDIO:
      prop_get = "get-audio-tags";
      prop_cur = "current-audio";
      prop_n = "n-audio";
      name = "audio";
      flag = 0x2;
      if (play->is_playbin3) {
        n = nb_audio;
        if (play->cur_video_sid) {
          selected_streams =
              g_list_append (selected_streams, play->cur_video_sid);
        }
        if (play->cur_text_sid) {
          selected_streams =
              g_list_append (selected_streams, play->cur_text_sid);
        }
      }
      break;
    case GST_PLAY_TRACK_TYPE_VIDEO:
      prop_get = "get-video-tags";
      prop_cur = "current-video";
      prop_n = "n-video";
      name = "video";
      flag = 0x1;
      if (play->is_playbin3) {
        n = nb_video;
        if (play->cur_audio_sid) {
          selected_streams =
              g_list_append (selected_streams, play->cur_audio_sid);
        }
        if (play->cur_text_sid) {
          selected_streams =
              g_list_append (selected_streams, play->cur_text_sid);
        }
      }
      break;
    case GST_PLAY_TRACK_TYPE_SUBTITLE:
      prop_get = "get-text-tags";
      prop_cur = "current-text";
      prop_n = "n-text";
      name = "subtitle";
      flag = 0x4;
      if (play->is_playbin3) {
        n = nb_text;
        if (play->cur_audio_sid) {
          selected_streams =
              g_list_append (selected_streams, play->cur_audio_sid);
        }
        if (play->cur_video_sid) {
          selected_streams =
              g_list_append (selected_streams, play->cur_video_sid);
        }
      }
      break;
    default:
      return;
  }

  if (!play->is_playbin3) {
    gint cur=0;
    g_object_get (play->playbin, prop_cur, &cur, prop_n, &n, "flags",
        &cur_flags, NULL);
  }

  index--;
  index = index < 0 ? 0 : index;
  index = index > n-1 ? n-1 : index;
  if (n < 1) {
    gst_print ("No %s tracks.\n", name);
    g_mutex_unlock (&play->selection_lock);
  } else {
    gchar *lcode = NULL, *lname = NULL;
    const gchar *lang = NULL;
    GstTagList *tags = NULL;

    if (index >= n && track_type != GST_PLAY_TRACK_TYPE_VIDEO) {
      index = -1;
      gst_print ("Disabling %s.           \n", name);
      if (play->is_playbin3) {
        /* Just make it empty for the track type */
      } else if (cur_flags & flag) {
        cur_flags &= ~flag;
        g_object_set (play->playbin, "flags", cur_flags, NULL);
      }
    } else {
      /* For video we only want to switch between streams, not disable it altogether */
      if (index >= n)
        index = 0;

      if (play->is_playbin3) {
        GstStream *stream;

        stream = play_get_nth_stream_in_collection (play, index, track_type);
        if (stream) {
          selected_streams = g_list_append (selected_streams,
              (gchar *) gst_stream_get_stream_id (stream));
          tags = gst_stream_get_tags (stream);
        } else {
          gst_print ("Collection has no stream for track %d of %d.\n",
              index + 1, n);
        }
      } else {
        if (!(cur_flags & flag) && track_type != GST_PLAY_TRACK_TYPE_VIDEO) {
          cur_flags |= flag;
          g_object_set (play->playbin, "flags", cur_flags, NULL);
        }
        g_signal_emit_by_name (play->playbin, prop_get, index, &tags);
      }

      if (tags != NULL) {
        gst_print ("\nGot tags %s\n\n\n\n", gst_tag_list_to_string(tags));
        if (gst_tag_list_get_string (tags, GST_TAG_LANGUAGE_CODE, &lcode))
          lang = gst_tag_get_language_name (lcode);
        else if (gst_tag_list_get_string (tags, GST_TAG_LANGUAGE_NAME, &lname))
          lang = lname;
        gst_tag_list_unref (tags);
      }
      if (lang != NULL)
        gst_print ("Switching to %s track %d of %d (%s).\n", name, index + 1, n,
            lang);
      else
        gst_print ("Switching to %s track %d of %d.\n", name, index + 1, n);
    }
    g_free (lcode);
    g_free (lname);
    g_mutex_unlock (&play->selection_lock);

    if (play->is_playbin3) {
      if (selected_streams)
        gst_element_send_event (play->playbin,
            gst_event_new_select_streams (selected_streams));
      else
        gst_print ("Can't disable all streams !\n");
    } else {
      g_object_set (play->playbin, prop_cur, index, NULL);
    }
  }

  if (selected_streams)
    g_list_free (selected_streams);
}

int aamp_get_media_track_num(AGMP_HANDLE handle, int* pn_video, int* pn_audio, int* pn_text)
{
  gint n_video, n_audio, n_text;

  CHECK_POINTER_VALID(handle);
  CHECK_POINTER_VALID(pn_video);
  CHECK_POINTER_VALID(pn_audio);
  CHECK_POINTER_VALID(pn_text);
  GstPlay* play = (GstPlay*)handle;

  /* Read some properties */
  g_object_get (play->playbin, "n-video", &n_video, NULL);
  g_object_get (play->playbin, "n-audio", &n_audio, NULL);
  g_object_get (play->playbin, "n-text", &n_text, NULL);

  *pn_video = n_video;
  *pn_audio = n_audio;
  *pn_text = n_text;

  return AAMP_SUCCESS;
}

int aamp_get_video_track_info(AGMP_HANDLE handle, int track_id, VideoInfo* video_info)
{
  CHECK_POINTER_VALID(handle);
  CHECK_POINTER_VALID(video_info);
  GstPlay* play = (GstPlay*)handle;
  GstTagList *tags;
  gchar *str, *total_str;

  video_info->track_id = track_id;
  tags = NULL;
  /* Retrieve the stream's video tags */
  g_signal_emit_by_name (play->playbin, "get-video-tags", track_id, &tags);
  if (tags) {
    total_str = g_strdup_printf ("video stream %d:\n", track_id);
    //gst_print ("%s\n", total_str);
    g_free (total_str);

    if (gst_tag_list_get_string (tags, GST_TAG_VIDEO_CODEC, &str)) {
      total_str = g_strdup_printf ("  codec: %s\n", str ? str : "unknown");
      //gst_print ("%s\n", total_str);
      memset(video_info->codec, 0, INFO_STRING_MAXLEN);
      strncpy(video_info->codec, str, INFO_STRING_MAXLEN-1);
      g_free (total_str);
      g_free (str);
    }
    if (gst_tag_list_get_string (tags, GST_TAG_CONTAINER_FORMAT, &str)) {
      total_str = g_strdup_printf ("  container: %s\n", str);
      //gst_print ("%s\n", total_str);
      memset(video_info->container, 0, INFO_STRING_MAXLEN);
      strncpy(video_info->container, str, INFO_STRING_MAXLEN-1);
      g_free (total_str);
      g_free (str);
    }
    gst_tag_list_free (tags);
  }

  GstPad *pad;
  g_signal_emit_by_name (play->playbin, "get-video-pad", track_id, &pad, NULL);
  if (pad != NULL) {
    gint width=0, height=0, framerate=0;
    GstCaps *caps = gst_pad_get_current_caps (pad);
    gst_structure_get_int (gst_caps_get_structure (caps, 0),"width", &width);
    gst_structure_get_int (gst_caps_get_structure (caps, 0),"height", &height);
    gint fr_num, fr_dem;
    gst_structure_get_fraction (gst_caps_get_structure (caps, 0),"framerate", &fr_num, &fr_dem);
    //gst_print ("width:%d, height:%d, framerate:%d\n", width, height, fr_num/fr_dem);
    video_info->width = width;
    video_info->height = height;
    video_info->framerate = (fr_dem==0 ? 0: fr_num/fr_dem);

    gst_caps_unref (caps);
    gst_object_unref (pad);
  }

  return AAMP_SUCCESS;
}

int aamp_get_audio_track_info(AGMP_HANDLE handle, int track_id, AudioInfo* audio_info)
{
  CHECK_POINTER_VALID(handle);
  CHECK_POINTER_VALID(audio_info);
  GstPlay* play = (GstPlay*)handle;
  GstTagList *tags;
  gchar *str, *total_str;
  guint rate = 0;

  audio_info->track_id = track_id;
  tags = NULL;
  /* Retrieve the stream's audio tags */
  g_signal_emit_by_name (play->playbin, "get-audio-tags", track_id, &tags);
  if (tags) {
    total_str = g_strdup_printf ("\naudio stream %d:\n", track_id);
    //gst_print ("%s\n", total_str);
    g_free (total_str);

    gchar* str1 = gst_tag_list_to_string (tags);
      //gst_print ("audio %d: %s\n", track_id, str1);
      g_free (str1);

    if (gst_tag_list_get_string (tags, GST_TAG_AUDIO_CODEC, &str)) {
      total_str = g_strdup_printf ("  codec: %s\n", str);
      //gst_print ("%s\n", total_str);
      memset(audio_info->codec, 0, INFO_STRING_MAXLEN);
      strncpy(audio_info->codec, str, INFO_STRING_MAXLEN-1);
      g_free (total_str);
      g_free (str);
    }
    if (gst_tag_list_get_string (tags, GST_TAG_LANGUAGE_NAME, &str)) {
      total_str = g_strdup_printf ("  language: %s\n", str);
      //gst_print ("%s\n", total_str);
      g_free (total_str);
      g_free (str);
    }
    if (gst_tag_list_get_uint (tags, GST_TAG_BITRATE, &rate)) {
      total_str = g_strdup_printf ("  bitrate: %d\n", rate);
      //gst_print ("%s\n", total_str);
      audio_info->rate = rate;
      g_free (total_str);
    }
    if (gst_tag_list_get_string (tags, GST_TAG_CONTAINER_FORMAT, &str)) {
      total_str = g_strdup_printf ("  container: %s\n", str);
      //gst_print ("%s\n", total_str);
      memset(audio_info->container, 0, INFO_STRING_MAXLEN);
      strncpy(audio_info->container, str, INFO_STRING_MAXLEN-1);
      g_free (total_str);
      g_free (str);
    }
    gst_tag_list_free (tags);
  }

  GstPad *pad;
  g_signal_emit_by_name (play->playbin, "get-audio-pad", track_id, &pad, NULL);
  if (pad != NULL) {
    gint samples=0, channels=0;
    GstCaps *caps = gst_pad_get_current_caps (pad);
    gst_structure_get_int (gst_caps_get_structure (caps, 0),"rate", &samples);
    gst_structure_get_int (gst_caps_get_structure (caps, 0),"channels", &channels);
    audio_info->channels = channels;
    audio_info->samples = samples;

    gst_caps_unref (caps);
    gst_object_unref (pad);
  }

  return AAMP_SUCCESS;
}

int aamp_get_text_track_info(AGMP_HANDLE handle, int track_id, TextInfo* text_info)
{
  CHECK_POINTER_VALID(handle);
  CHECK_POINTER_VALID(text_info);
  GstPlay* play = (GstPlay*)handle;
  GstTagList *tags;
  gchar *str, *total_str;

  text_info->track_id = track_id;
  tags = NULL;
  /* Retrieve the stream's subtitle tags */
  g_signal_emit_by_name (play->playbin, "get-text-tags", track_id, &tags);
  if (tags) {
    total_str = g_strdup_printf ("\nsubtitle stream %d:\n", track_id);
    //gst_print ("%s\n", total_str);
    g_free (total_str);
    if (gst_tag_list_get_string (tags, GST_TAG_LANGUAGE_CODE, &str)) {
      total_str = g_strdup_printf ("  language: %s\n", str);
      //gst_print ("%s\n", total_str);
      memset(text_info->lang, 0, INFO_STRING_MAXLEN);
      strncpy(text_info->lang, str, INFO_STRING_MAXLEN-1);
      g_free (total_str);
      g_free (str);
    }
    else {
      memset(text_info->lang, 0, INFO_STRING_MAXLEN);
      strcpy(text_info->lang, "unknown");
    }
    gst_tag_list_free (tags);
  }

  /*GstPad *pad;
  g_signal_emit_by_name (play->playbin, "get-text-pad", track_id, &pad, NULL);
  if (pad != NULL) {
    gint samples=0, channels=0;
    GstCaps *caps = gst_pad_get_current_caps (pad);
    str = gst_structure_get_string (gst_caps_get_structure (caps, 0),"format");
    memset(text_info->format, 0, INFO_STRING_MAXLEN);
    strncpy(text_info->format, str, INFO_STRING_MAXLEN-1);
    g_free (str);

    gst_caps_unref (caps);
    gst_object_unref (pad);
  }*/

  return AAMP_SUCCESS;
}

int agmp_set_window_size(AGMP_HANDLE handle, int x, int y, int w, int h)
{
  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;

  player->win_size.x = x;
  player->win_size.y = y;
  player->win_size.w = w;
  player->win_size.h = h;

  if (NULL == player->vsink) {
    gst_print ("can't find vsink.\n");
    return AAMP_FAILED;
  }

  gst_print ("w:%d, h:%d\n", player->win_size.w, player->win_size.h);
  if (player->win_size.w > 0 && player->win_size.h > 0) {
    char videoRectangle[32] = {0};
    sprintf(videoRectangle, "%d,%d,%d,%d", player->win_size.x,player->win_size.y,player->win_size.w,player->win_size.h);
    memcpy(player->videoRectangle, videoRectangle, 32);
    g_object_set (player->vsink, "rectangle", player->videoRectangle, NULL);
    gst_print ("set window size %s\n", videoRectangle);
  }

  return AAMP_SUCCESS;
}

int agmp_get_window_size(AGMP_HANDLE handle, int* x, int* y, int* w, int* h)
{
  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;

  *x = player->win_size.x;
  *y = player->win_size.y;
  *w = player->win_size.w;
  *h = player->win_size.h;
  return AAMP_SUCCESS;
}

int agmp_set_zoom(AGMP_HANDLE handle, int zoom)
{
  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;
  if (!player->vsink)
  {
    return AAMP_FAILED;
  }
  g_object_set(player->vsink, "scale-mode", 0 == zoom ? 0 : 3, NULL);
  return AAMP_SUCCESS;
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

static int porting_timeout (void* handle)
{
  if (NULL == handle) {
    return TRUE;
  }

  GstPlay* player = (GstPlay*)handle;
  gint64 pos = -1, dur = -1;

  if (player->buffering)
    return TRUE;

  dur = agmp_get_duration(handle);
  pos = agmp_get_position(handle);

  if (pos >= 0 && dur > 0) {
    gchar dstr[32], pstr[32];

    /* FIXME: pretty print in nicer format */
    g_snprintf (pstr, 32, "%" GST_TIME_FORMAT, GST_TIME_ARGS (pos));
    pstr[9] = '\0';
    g_snprintf (dstr, 32, "%" GST_TIME_FORMAT, GST_TIME_ARGS (dur));
    dstr[9] = '\0';
    gst_print ("%s / %s\r", pstr, dstr);
  }

  return TRUE;
}

unsigned int aamp_create_timer(unsigned int interval, timeout_callback callback, AGMP_HANDLE handle)
{
	return g_timeout_add (interval, porting_timeout, handle);
}

void aamp_destroy_timer(unsigned int timer_id)
{
	g_source_remove (timer_id);
}

int aamp_register_events(AGMP_HANDLE handle, message_callback callback, void* userdata)
{
  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;

  player->notify_app = callback;
  player->userdata = userdata;
  return AAMP_SUCCESS;
}
