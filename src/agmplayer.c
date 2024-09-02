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
#include <gst/allocators/gstsecmemallocator.h>
#include "agmplayer.h"

#define PROGRESS_CALLBACK_CNT 10

GST_DEBUG_CATEGORY (agmp_debug);
#define GST_CAT_DEFAULT agmp_debug

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

typedef enum {
  GST_PLAY_FLAG_VIDEO         = (1 << 0),
  GST_PLAY_FLAG_AUDIO         = (1 << 1),
  GST_PLAY_FLAG_TEXT          = (1 << 2),
  GST_PLAY_FLAG_VIS           = (1 << 3),
  GST_PLAY_FLAG_SOFT_VOLUME   = (1 << 4),
  GST_PLAY_FLAG_NATIVE_AUDIO  = (1 << 5),
  GST_PLAY_FLAG_NATIVE_VIDEO  = (1 << 6),
  GST_PLAY_FLAG_DOWNLOAD      = (1 << 7),
  GST_PLAY_FLAG_BUFFERING     = (1 << 8),
  GST_PLAY_FLAG_DEINTERLACE   = (1 << 9),
  GST_PLAY_FLAG_SOFT_COLORBALANCE = (1 << 10),
  GST_PLAY_FLAG_FORCE_FILTERS = (1 << 11),
  GST_PLAY_FLAG_FORCE_SW_DECODERS = (1 << 12),
} GstPlayFlags;

typedef struct
{
  gchar *uri;
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
  unsigned int timer_id;
  int timer_cnt;
  message_callback notify_app;
  WindowSize win_size;
  int percent;
  gboolean async_done;

  gboolean buffering;
  gboolean is_live;

  GstState desired_state;       /* as per user interaction, PAUSED or PLAYING */

  /* configuration */
  gboolean gapless;
  gboolean wait_on_eos;

  GstPlayTrickMode trick_mode;
  gdouble rate;
  double volume;

  /* secmem */
  GMutex lock;
  GstAllocator* allocator;

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

static gboolean play_bus_msg (GstBus * bus, GstMessage * msg, gpointer data);
static int play_reset (GstPlay * player);
static gboolean play_do_seek (GstPlay * play, gint64 pos, gdouble rate, GstPlayTrickMode mode);
static void default_element_added(GstBin *bin, GstElement *element, gpointer user_data);
void aamp_switch_trick_mode (GstPlay * play);
int get_audio_track_num(GstPlay * play);
static void play_track_selection (GstPlay * play, GstPlayTrackType track_type, gint index);
static void play_set_playback_rate (GstPlay * play, gdouble rate);
static int agmp_replay (AGMP_HANDLE handle);
static void set_aamp_state(GstPlay *player, PrivAAMPState state);
static GstAllocator* handle_need_allocator(GstElement *wlcdmi, gboolean is_4k, guint decoder_format, gpointer user_data);
static void element_setup (GstElement *playbin, GstElement *element, gpointer user_data);
static int porting_timeout (void* handle);


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

static GstAllocator* handle_need_allocator(GstElement *wlcdmi, gboolean is_4k, guint decoder_format, gpointer user_data)
{
  GstPlay *play;
  if (NULL == user_data)
  {
    GST_ERROR ("user_data is null.");
    return NULL;
  }
  play = (GstPlay *)user_data;

  g_mutex_lock(&play->lock);
  if (!play->allocator) {
      play->allocator = gst_secmem_allocator_new(is_4k, decoder_format);
      GST_DEBUG ("allocator new %p.", play->allocator);
  }
  g_mutex_unlock(&play->lock);

  return play->allocator;
}

static void element_setup (GstElement *playbin, GstElement *element, gpointer user_data)
{
  GstElementFactory *f = gst_element_get_factory(element);
  if (!f)
      return;

  if (!strcmp(GST_OBJECT_NAME (f), "wlcdmi")) {
      g_signal_connect (G_OBJECT(element), "need-allocator", (GCallback) handle_need_allocator, user_data);
      {
        g_object_set (element, "external-allocator", TRUE, NULL);
      }
  } else if (!strcmp(GST_OBJECT_NAME (f), "hlsdemux")) {
    GST_DEBUG ("use-hw-decrypt.");
    g_object_set (element, "use-hw-decrypt", TRUE, NULL);
  }
}

static void default_element_added(GstBin *bin, GstElement *element, gpointer user_data)
{
  GstPlay *play;
  if (NULL == user_data)
  {
    GST_ERROR ("user_data is null.");
    return;
  }

  play = (GstPlay *)user_data;

  GST_DEBUG("New element added to %s : %s", GST_ELEMENT_NAME(bin), GST_ELEMENT_NAME(element));

  if (g_strrstr(GST_ELEMENT_NAME(element), "uridecodebin"))
  {
      if (play->uridb != element)
      {
          g_signal_connect(element, "element-added", G_CALLBACK(default_element_added), play);
      }
      play->uridb = element;
  }
  else if (g_strrstr(GST_ELEMENT_NAME(element), "decodebin"))
  {
      if (play->db != element)
      {
          g_signal_connect(element, "element-added", G_CALLBACK(default_element_added), play);
      }
      play->db = element;
  }
  else if (g_strrstr(GST_ELEMENT_NAME(element), "parsebin"))
  {
      if (play->pb != element)
      {
          g_signal_connect(element, "element-added", G_CALLBACK(default_element_added), play);
      }
      play->pb = element;
  }
  else if (g_strrstr(GST_ELEMENT_NAME(element), "vbin"))
  {
      if (play->vbin != element)
      {
          g_signal_connect(element, "element-added", G_CALLBACK(default_element_added), play);
      }
      play->vbin = element;
  }
  else if (g_strrstr(GST_ELEMENT_NAME(element), "abin"))
  {
      if (play->abin != element)
      {
          g_signal_connect(element, "element-added", G_CALLBACK(default_element_added), play);
      }
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
          g_object_set (play->vdec, "enable-nr", TRUE, NULL);
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
    GST_ERROR ("player is null.");
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
      GST_ERROR ("pointer is null."); \
      return AAMP_NULL_POINTER; \
    } \
  } while(0);

static void video_underflow(gpointer handle)
{
  GstPlay *player = handle;
  if (NULL == handle)
  {
    GST_ERROR ("handle is null.");
    return;
  }
  callback_to_app(player, AGMP_MESSAGE_VIDEO_UNDERFLOW, player->userdata);
}

static void video_first_frame(gpointer handle)
{
  GstPlay *player = handle;
  if (NULL == handle)
  {
    GST_ERROR ("handle is null.");
    return;
  }
  callback_to_app(player, AGMP_MESSAGE_FIRST_VFRAME, player->userdata);
}

static void audio_underflow(gpointer handle)
{
  GstPlay *player = handle;
  if (NULL == handle)
  {
    GST_ERROR ("handle is null.");
    return;
  }
  callback_to_app(player, AGMP_MESSAGE_AUDIO_UNDERFLOW, player->userdata);
}

/*
static void audio_first_frame(gpointer user_data)
{
  if (NULL == user_data)
  {
    GST_ERROR ("user_data is null.");
    return;
  }
  GstPlay *player = user_data;
  callback_to_app(player, AGMP_MESSAGE_FIRST_AFRAME, player->userdata);
}
*/

int agmp_set_uri(AGMP_HANDLE handle, const char* uri)
{
  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;
  player->uri = g_strdup (uri);
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
  GstPlay *player = data;
  if (NULL == data)
  {
    GST_ERROR ("play thread failed.");
    return NULL;
  }

  GST_DEBUG ("play thread enter.");
  //block here
  g_main_loop_run (player->loop);
  GST_DEBUG ("play thread quit.");

  return NULL;
}

int agmp_set_log_level (LOG_LEVEL level)
{
  L.level = level;
  return AAMP_SUCCESS;
}

AGMP_HANDLE agmp_init (void)
{
  int argc = 0;
  char **argv = NULL;
  GstPlay *player;
  gboolean use_playbin3 = FALSE;
  gchar *flags_string = NULL;
  const gchar *sink_name;
  char* audio_sink = "amlhalasink";
  char* video_sink = "westerossink";
  GstElement *playbin = NULL;
  GstElement *sink = NULL;

  GST_DEBUG("agmp_init in");

  gst_init(&argc, &argv);
  GST_DEBUG_CATEGORY_INIT (agmp_debug, "agmp", 0, "amlogic gstreamer media player");

  player = g_new0 (GstPlay, 1);
  if (NULL == player)
  {
    GST_ERROR ("new player failed.");
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
  player->loop = NULL;
  player->bus_watch = 0;
  player->notify_app = NULL;
  player->userdata = NULL;

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
  g_mutex_init(&player->lock);
  player->allocator = NULL;

  sink_name = g_getenv ("GST_CFG_VIDEO_SINK");
  if (sink_name)
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

  if (use_playbin3) {
    playbin = gst_element_factory_make ("playbin3", "playbin");
  } else {
    playbin = gst_element_factory_make ("playbin", "playbin");
  }
  if (playbin == NULL) {
    GST_ERROR ("make playbin failed.");
    return NULL;
  }

  player->playbin = playbin;
  g_signal_connect(playbin, "element-added", G_CALLBACK(default_element_added), player);
  g_signal_connect (playbin, "element-setup", G_CALLBACK (element_setup), player);

  if (use_playbin3) {
    player->is_playbin3 = TRUE;
  } else {
    const gchar *env = g_getenv ("USE_PLAYBIN3");
    if (env && g_str_has_prefix (env, "1"))
      player->is_playbin3 = TRUE;
  }

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
      GST_WARNING ("Couldn't create specified audio sink '%s'", audio_sink);
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
      GST_WARNING ("Couldn't create specified video sink '%s'", video_sink);
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
      GST_ERROR ("Couldn't convert '%s' to playbin flags!", flags_string);
    g_value_unset (&val);
  }
  else
  {
    gint default_flags = GST_PLAY_FLAG_AUDIO | GST_PLAY_FLAG_VIDEO | GST_PLAY_FLAG_TEXT | GST_PLAY_FLAG_NATIVE_VIDEO;
    //g_object_set(player->playbin, "flags", default_flags, NULL);
    GST_DEBUG ("do not set default flag 0x%x", default_flags);
  }

  player->loop = g_main_loop_new (NULL, FALSE);
  player->bus_watch = gst_bus_add_watch (GST_ELEMENT_BUS (player->playbin), play_bus_msg, player);

  //create play thread
  player->play_thread = g_thread_new ("video play run thread", play_run_thread, player);
  if (!player->play_thread) {
      GST_ERROR ("fail to create play thread");
      return NULL;
  }

  player->timer_id = g_timeout_add (100, porting_timeout, player);
  player->timer_cnt = PROGRESS_CALLBACK_CNT;

  L.level = LOG_DEBUG;

  return (AGMP_HANDLE)player;
}

int agmp_prepare (AGMP_HANDLE handle)
{
  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;
  gboolean ret = TRUE;

  GST_DEBUG ("agmp_prepare in");

  set_aamp_state(player, eSTATE_PREPARING);
  if (AGMP_STATUS_PREPARED == player->status)
  {
    GST_ERROR ("already playing: %d.", player->status);
    return AAMP_SUCCESS;
  }

  if (player->status != AGMP_STATUS_NULL && player->status != AGMP_STATUS_STOPED)
  {
    GST_ERROR ("can't be called in this state: %d.", player->status);
    return AAMP_FAILED_IN_THIS_STATE;
  }

  play_reset (player);
  g_object_set (player->playbin, "uri", player->uri, NULL);

  player->async_done = FALSE;

  switch (gst_element_set_state (player->playbin, GST_STATE_PAUSED)) {
  case GST_STATE_CHANGE_FAILURE:
    GST_ERROR ("Pipeline state change fail.");
    /* ignore, we should get an error message posted on the bus */
    ret = FALSE;
    break;
  case GST_STATE_CHANGE_NO_PREROLL:
    GST_DEBUG ("Pipeline is live.");
    player->is_live = TRUE;
    break;
  case GST_STATE_CHANGE_ASYNC:
    GST_DEBUG ("Prerolling...");
    break;
  default:
    GST_DEBUG ("Pipeline to paused.");
    break;
  }

  if (!ret)
    return AAMP_FAILED;

  player->status = AGMP_STATUS_PREPARED;
  GST_DEBUG ("prepare stream done.");
  set_aamp_state(player, eSTATE_PREPARED);

  return AAMP_SUCCESS;
}

int agmp_play (AGMP_HANDLE handle)
{
  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;

  GST_DEBUG ("agmp_play in");

  if (AGMP_STATUS_PLAYING == player->status)
  {
    GST_ERROR ("already playing: %d.", player->status);
    return AAMP_SUCCESS;
  }

  if (player->status != AGMP_STATUS_PREPARED && player->status != AGMP_STATUS_PAUSED && player->status != AGMP_STATUS_STOPED)
  {
    GST_ERROR ("can't be called in this state: %d.", player->status);
    return AAMP_FAILED_IN_THIS_STATE;
  }

  player->desired_state = GST_STATE_PLAYING;
  gst_element_set_state (player->playbin, GST_STATE_PLAYING);

  return AAMP_SUCCESS;
}

int agmp_pause (AGMP_HANDLE handle)
{
  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;

  GST_DEBUG ("agmp_pause in");

  if (AGMP_STATUS_PAUSED == player->status)
  {
    GST_ERROR ("already paused: %d.", player->status);
    return AAMP_SUCCESS;
  }

  if (player->status != AGMP_STATUS_PLAYING)
  {
    GST_ERROR ("can't be called in this state: %d.", player->status);
    return AAMP_FAILED_IN_THIS_STATE;
  }

  if (player->buffering) {
    GST_ERROR ("buffering, no need pause");
    return AAMP_SUCCESS;
  }

  player->desired_state = GST_STATE_PAUSED;
  gst_element_set_state (player->playbin, GST_STATE_PAUSED);

  return AAMP_SUCCESS;
}

int agmp_stop (AGMP_HANDLE handle)
{
  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;

  GST_DEBUG("agmp_stop in");

  if (AGMP_STATUS_STOPED == player->status)
  {
    GST_ERROR ("already stoped: %d.", player->status);
    return AAMP_SUCCESS;
  }

  if (player->status != AGMP_STATUS_PREPARED && player->status != AGMP_STATUS_PLAYING && player->status != AGMP_STATUS_PAUSED)
  {
    GST_ERROR ("can't be called in this state: %d.", player->status);
    return AAMP_FAILED_IN_THIS_STATE;
  }

  set_aamp_state(player, eSTATE_STOPPING);
  gst_element_set_state (player->playbin, GST_STATE_READY);
  if (player->allocator) {
    gst_object_unref(player->allocator);
    player->allocator = NULL;
  }

  // wait state change
  g_usleep(1000000);
  set_aamp_state(player, eSTATE_STOPPED);
  player->status = AGMP_STATUS_STOPED;

  return AAMP_SUCCESS;
}

void quit_thread(GstPlay* player)
{
  GST_DEBUG("quit_thread in");

  if (player->play_thread) {
    GST_DEBUG ("join thread\n");
    g_thread_join (player->play_thread);
    player->play_thread = NULL;
  }
}

void quit_loop(GstPlay* player)
{
  GST_DEBUG("to quit main loop");

  g_main_loop_quit (player->loop);
}

int agmp_exit (AGMP_HANDLE handle)
{
  GST_DEBUG("agmp_exit in");

  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;

  set_aamp_state(player, eSTATE_RELEASED);

  quit_loop(player);
  agmp_deinit(handle);
  quit_thread(player);
  g_source_remove (player->timer_id);
  g_free (player);
  player = NULL;

  //gst_deinit();

  GST_DEBUG ("agmp_exit out");

  return AAMP_SUCCESS;
}

void agmp_deinit (AGMP_HANDLE handle)
{
  GST_DEBUG("agmp_deinit %p in", handle);

  GstPlay* player = (GstPlay*)handle;

  if (!player)
    return;

  play_reset (player);

  gst_element_set_state (player->playbin, GST_STATE_NULL);
  gst_object_unref (player->playbin);

  g_source_remove (player->bus_watch);

  g_main_loop_unref (player->loop);

  //g_strfreev (player->uri);

  if (player->collection)
    gst_object_unref (player->collection);
  if (player->cur_audio_sid != NULL)
    g_free (player->cur_audio_sid);
  if (player->cur_video_sid != NULL)
    g_free (player->cur_video_sid);
  if (player->cur_text_sid != NULL)
    g_free (player->cur_text_sid);
  if (player->uri != NULL)
    g_free (player->uri);
  g_mutex_clear (&player->selection_lock);

}

AGMP_SSTATUS agmp_get_state(AGMP_HANDLE handle)
{
  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;

  GST_DEBUG("status=%d", player->status);

  return player->status;
}

unsigned int agmp_get_aamp_state(AGMP_HANDLE handle)
{
  GST_TRACE("trace in");

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

  GST_DEBUG("state = %d", state);

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

  gint64  pos = -1;
  if (player->buffering)
    return -1;

  gst_element_query_position (player->playbin, GST_FORMAT_TIME, &pos);

  return (long long)pos;
}

long long agmp_get_duration(AGMP_HANDLE handle)
{
  if (NULL == handle)
  {
    return -1;
  }
  GstPlay* player = (GstPlay*)handle;

  gint64 dur = -1;
  if (player->buffering)
    return -1;

  gst_element_query_duration (player->playbin, GST_FORMAT_TIME, &dur);

  return (long long)dur;
}

int agmp_set_speed(AGMP_HANDLE handle, AGMP_PLAY_SPEED rate)
{
  GST_TRACE("trace in");

  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;

  if (player->status != AGMP_STATUS_PLAYING)
  {
    GST_ERROR ("can't be called in this state: %d.", player->status);
    return AAMP_FAILED_IN_THIS_STATE;
  }

  double rate_level[] = {0.125, 0.25, 0.5, 1, 2, 4, 8};
  if (rate > sizeof(rate_level)/sizeof(rate_level[0])) {
    GST_ERROR ("rate out of range, %d.", rate);
    return AAMP_INVALID_PARAM;
  }

  double new_rate = rate_level[rate];

  if (new_rate != player->rate)
  {
    player->rate = new_rate;
    GST_DEBUG("set rate to %lf", player->rate);
    play_set_playback_rate (player, player->rate);
  }
  else
  {
    GST_DEBUG("no need to set rate %lf", player->rate);
  }
  return AAMP_SUCCESS;
}

int agmp_get_speed(AGMP_HANDLE handle)
{
  GST_TRACE("trace in");

  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;
  return player->rate;
}

/* reset for new file/stream */
static int play_reset (GstPlay * player)
{
  CHECK_POINTER_VALID(player);

  GST_DEBUG("play_reset in");

  player->buffering = FALSE;
  player->is_live = FALSE;
  return AAMP_SUCCESS;
}

int agmp_replay (AGMP_HANDLE handle)
{
  GST_TRACE("trace in");

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
      GST_DEBUG ("Pipeline is live.");
      player->is_live = TRUE;
      break;
    case GST_STATE_CHANGE_ASYNC:
      GST_DEBUG ("Prerolling...");
      break;
    default:
      break;
  }

  return agmp_play(player);
}

int aamp_set_audio_track(AGMP_HANDLE handle, int trackid)
{
  GST_DEBUG("audio track=%d", trackid);
  play_track_selection (handle, GST_PLAY_TRACK_TYPE_AUDIO, (gint)trackid);
  return AAMP_SUCCESS;
}

int set_video_track(AGMP_HANDLE handle, int trackid)
{
  GST_DEBUG("video track=%d", trackid);
  play_track_selection (handle, GST_PLAY_TRACK_TYPE_AUDIO, (gint)trackid);
  return AAMP_SUCCESS;
}

int set_subtitle_track(AGMP_HANDLE handle, int trackid)
{
  GST_DEBUG("subtitle track=%d", trackid);
  play_track_selection (handle, GST_PLAY_TRACK_TYPE_AUDIO, (gint)trackid);
  return AAMP_SUCCESS;
}

int get_audio_track_num(GstPlay * player)
{
  GST_DEBUG("get_audio_track_num in");

  CHECK_POINTER_VALID(player);
  /* playbin3 variables */
  gint nb_audio = 0, nb_video = 0, nb_text = 0;

  g_mutex_lock (&player->selection_lock);
  if (player->is_playbin3) {
    if (!player->collection) {
      gst_print ("No stream-collection\n");
      g_mutex_unlock (&player->selection_lock);
      return 0;
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
  GST_INFO (
      "audio track number:%d\n" \
      "video track number:%d\n" \
      "subtitle track number:%d\n",
      nb_audio, nb_video, nb_text
  );
  return nb_audio;
}

int agmp_seek(AGMP_HANDLE handle, double position)
{
  GST_TRACE("trace in");

  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;
  GstQuery *query;
  gboolean seekable = FALSE;

  GST_DEBUG("seek to %lf", position);
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
    GST_DEBUG ("Reached end of play list.");
    agmp_stop(player);
  } else {
    if (pos < 0)
      pos = 0;
    play_do_seek (player, pos, player->rate, player->trick_mode);
  }

  return AAMP_SUCCESS;

seek_failed:
  GST_ERROR ("Could not seek");

  return AAMP_FAILED;
}

/*int enable_keep_last_frame(GstPlay * player, gboolean enable)
{
  player->wait_on_eos = enable;
}*/

int agmp_get_buffering_percent(AGMP_HANDLE handle)
{
  GST_TRACE("trace in");

  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;
  return player->percent;
}

static gboolean play_bus_msg (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstPlay *player = user_data;

  if (player == NULL) {
    return TRUE;
  }

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ASYNC_DONE:

      /* dump graph on preroll */
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (player->playbin),
          GST_DEBUG_GRAPH_SHOW_ALL, "agmplayer.async-done");

      GST_DEBUG ("Prerolled.");

      player->async_done = TRUE;
      //notify app
      callback_to_app(player, AGMP_MESSAGE_ASYNC_DONE, player->userdata);
      break;
    case GST_MESSAGE_BUFFERING:
    {
      gint percent;
      gst_message_parse_buffering (msg, &percent);

      //notify app
      player->percent = percent;
      callback_to_app(player, AGMP_MESSAGE_BUFFERING, player->userdata);
      break;
    }
    case GST_MESSAGE_EOS:
      //notify app
      callback_to_app(player, AGMP_MESSAGE_EOS, player->userdata);
      break;
    case GST_MESSAGE_WARNING:
    {
      GError *err;
      gchar *dbg = NULL;

      // dump graph on warning
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (player->playbin),
          GST_DEBUG_GRAPH_SHOW_ALL, "agmplayer.warning");

      gst_message_parse_warning (msg, &err, &dbg);
      GST_WARNING ("WARNING %s", err->message);
      if (dbg != NULL)
        GST_ERROR ("WARNING debug information: %s", dbg);
      g_clear_error (&err);
      g_free (dbg);

      break;
    }
    case GST_MESSAGE_ERROR:
    {
      GError *err;
      gchar *dbg;

      // dump graph on error
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (player->playbin),
          GST_DEBUG_GRAPH_SHOW_ALL, "agmplayer.error");

      gst_message_parse_error (msg, &err, &dbg);
      GST_ERROR ("ERROR %s for %s", err->message, player->uri);
      if (dbg != NULL)
        GST_ERROR ("ERROR debug information: %s", dbg);
      g_clear_error (&err);
      g_free (dbg);

      // flush any other error messages from the bus and clean up
      gst_element_set_state (player->playbin, GST_STATE_NULL);

      //notify app
      callback_to_app(player, AGMP_MESSAGE_ERROR, player->userdata);
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
      GST_DEBUG ("stream collect done.");
      break;
    }
    case GST_MESSAGE_STREAMS_SELECTED:
    {
      GstStreamCollection *collection = NULL;
      guint i, len;

      GST_DEBUG ("SELECTED msg");

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
              GST_ERROR ("Unknown stream type with stream-id %s", stream_id);
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
          GST_DEBUG ("bus message status change to pending, %p", player->playbin);
          GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (player->playbin),
              GST_DEBUG_GRAPH_SHOW_ALL, "agmplayer.pending");
        }
        else if (state == GST_STATE_NULL) {
          GST_DEBUG ("bus message status change to null, %p", player->playbin);
        }
        else if (state == GST_STATE_READY) {
          GST_DEBUG ("bus message status change to ready, %p", player->playbin);
          GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (player->playbin),
              GST_DEBUG_GRAPH_SHOW_ALL, "agmplayer.ready");
        }
        else if (state == GST_STATE_PAUSED) {
          GST_DEBUG ("bus message status change to paused, %p", player->playbin);
          GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (player->playbin),
              GST_DEBUG_GRAPH_SHOW_ALL, "agmplayer.paused");
          player->status = AGMP_STATUS_PAUSED;
          set_aamp_state(player, eSTATE_PAUSED);
        }
        else if (state == GST_STATE_PLAYING) {
          GST_DEBUG ("bus message status change to playing, %p", player->playbin);
          GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (player->playbin),
              GST_DEBUG_GRAPH_SHOW_ALL, "agmplayer.playing");
          player->status = AGMP_STATUS_PLAYING;
          set_aamp_state(player, eSTATE_PLAYING);
        }
        callback_to_app(player, AGMP_MESSAGE_STATE_CHANGE, player->userdata);
      }
      break;
    }
    default:
      GST_DEBUG("not handle msg type=%d", GST_MESSAGE_TYPE (msg));
      break;
  }

  return TRUE;
}

int agmp_set_volume(AGMP_HANDLE handle, double volume)
{
  GST_TRACE("trace in");

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
    GST_ERROR ("set volume failed, asink is null.");
    return AAMP_FAILED;
  }
  player->volume = volume;
  g_object_set(player->asink, "stream-volume", player->volume, NULL);
  gst_print ("set volume: %.0f%%\n", player->volume  * 100);
  return AAMP_SUCCESS;
}

double agmp_get_volume(AGMP_HANDLE handle)
{
  GST_TRACE("trace in");

  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;
  return player->volume * 100;
}

int agmp_set_video_mute(AGMP_HANDLE handle, int mute)
{
  GST_TRACE("trace in");

  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;
  player->video_muted = mute;
  if (!player->vsink) {
    GST_ERROR ("set video mute failed, vsink is null.");
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

  GST_TRACE("trace in");

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

  GST_TRACE("trace in");

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
  GST_TRACE("trace in");

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
  GST_TRACE("trace in");

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

  GST_TRACE("trace in");

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

  GST_TRACE("trace in");

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
  /* playbin3 variables */
  GList *selected_streams = NULL;
  gint nb_audio = 0, nb_video = 0, nb_text = 0;
  guint len, i;

  GST_DEBUG("play_track_selection in");

  if (!play)
    return;

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

  GST_INFO("video num=%d, audio num=%d, text num=%d", n_video, n_audio, n_text);

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

  GST_DEBUG("video track id = %d", track_id);

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
      GST_INFO("%s", total_str);

      memset(video_info->codec, 0, INFO_STRING_MAXLEN);
      strncpy(video_info->codec, str, INFO_STRING_MAXLEN-1);
      g_free (total_str);
      g_free (str);
    }
    if (gst_tag_list_get_string (tags, GST_TAG_CONTAINER_FORMAT, &str)) {
      total_str = g_strdup_printf ("  container: %s\n", str);
      GST_INFO("container = %s", total_str);

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
    gint width=0, height=0;
    GstCaps *caps = gst_pad_get_current_caps (pad);
    gst_structure_get_int (gst_caps_get_structure (caps, 0),"width", &width);
    gst_structure_get_int (gst_caps_get_structure (caps, 0),"height", &height);
    gint fr_num, fr_dem;
    gst_structure_get_fraction (gst_caps_get_structure (caps, 0),"framerate", &fr_num, &fr_dem);
    GST_INFO("width=%d, height=%d, framerate=%d:%d\n", width, height, fr_num, fr_dem);

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

  GST_DEBUG("audio track id = %d", track_id);

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
      GST_INFO("%s", total_str);

      memset(audio_info->codec, 0, INFO_STRING_MAXLEN);
      strncpy(audio_info->codec, str, INFO_STRING_MAXLEN-1);
      g_free (total_str);
      g_free (str);
    }
    if (gst_tag_list_get_string (tags, GST_TAG_LANGUAGE_NAME, &str)) {
      total_str = g_strdup_printf ("  language: %s\n", str);
      GST_INFO("%s", total_str);

      g_free (total_str);
      g_free (str);
    }
    if (gst_tag_list_get_uint (tags, GST_TAG_BITRATE, &rate)) {
      total_str = g_strdup_printf ("  bitrate: %d\n", rate);
      GST_INFO("%s", total_str);

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

  GST_DEBUG("text track id = %d", track_id);

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
      GST_INFO("text language = %s", total_str);

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
    GST_ERROR ("can't find vsink.");
    return AAMP_FAILED;
  }

  GST_DEBUG ("w=%d, h=%d", player->win_size.w, player->win_size.h);
  if (player->win_size.w > 0 && player->win_size.h > 0) {
    char videoRectangle[32] = {0};
    sprintf(videoRectangle, "%d,%d,%d,%d", player->win_size.x,player->win_size.y,player->win_size.w,player->win_size.h);
    memcpy(player->videoRectangle, videoRectangle, 32);
    g_object_set (player->vsink, "rectangle", player->videoRectangle, NULL);
    GST_DEBUG ("set window size %s", videoRectangle);
  }

  return AAMP_SUCCESS;
}

int agmp_get_window_size(AGMP_HANDLE handle, int* x, int* y, int* w, int* h)
{
  GST_TRACE("trace in");

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
  GST_TRACE("trace in");

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

static int porting_timeout (void* handle)
{
  if (NULL == handle) {
    return TRUE;
  }

  GstPlay* player = (GstPlay*)handle;
  long long pos = -1, dur = -1;

  if (player->buffering)
    return TRUE;

  dur = agmp_get_duration(handle);
  pos = agmp_get_position(handle);

  player->timer_cnt++;
  if (pos >= 0 && dur > 0) {
    gchar dstr[32], pstr[32];

    /* FIXME: pretty print in nicer format */
    g_snprintf (pstr, 32, "%" GST_TIME_FORMAT, GST_TIME_ARGS ((gint64)pos));
    pstr[9] = '\0';
    g_snprintf (dstr, 32, "%" GST_TIME_FORMAT, GST_TIME_ARGS ((gint64)dur));
    dstr[9] = '\0';
    g_print ("%s / %s\r", pstr, dstr);

    // progress update call back
    if (player->timer_cnt >= PROGRESS_CALLBACK_CNT) {
      player->timer_cnt = 0;
      callback_to_app(player, AGMP_MESSAGE_PROGRESS_UPDATE, player->userdata);
    }
  }

  return TRUE;
}


int aamp_register_events(AGMP_HANDLE handle, message_callback callback, void* userdata)
{
//  GST_TRACE("trace in");

  CHECK_POINTER_VALID(handle);
  GstPlay* player = (GstPlay*)handle;

  player->notify_app = callback;
  player->userdata = userdata;
  return AAMP_SUCCESS;
}
