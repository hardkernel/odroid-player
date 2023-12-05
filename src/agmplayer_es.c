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
#include <gst/app/gstappsrc.h>
#include <gst/pbutils/pbutils.h>
#include <gst/tag/tag.h>
#include <gst/math-compat.h>
#include <gst/base/gstbytewriter.h>
#include <gst/allocators/gstsecmemallocator.h>

#include "agmplayer_es.h"
// #include "agmplayer_es_secure.h"
#include "agmplayer_es_video_color_metadata_internal.h"

GST_DEBUG_CATEGORY(agmp_es_debug);
#define GST_CAT_DEFAULT agmp_es_debug

#define AGMP_ES_DEFAULT_VID_SRC_SIZE 32 * 1024 * 1024 // 32Mb
#define AGMP_ES_DEFAULT_AUD_SRC_SIZE 8 * 1024 * 1024  // 8Mb
#define AGMP_ES_DEFAULT_VID_SRC_MIN_PERCENT 50        // 50%
#define AGMP_ES_DEFAULT_AUD_SRC_MIN_PERCENT 50        // 50%
#define AGMP_ES_DEFAULT_VID_SRC_LOW_PERCENT 10        // 10%
#define AGMP_ES_DEFAULT_AUD_SRC_LOW_PERCENT 10        // 10%

#define AGMP_ES_CHECK_BUF_INTERVAL 10              // ms
#define AGMP_ES_MIN_VID_BUF_TIME 250               // ms
#define AGMP_ES_MIN_AUD_BUF_TIME 250               // ms
#define AGMP_ES_MAX_VID_BUF_TIME 2000              // ms
#define AGMP_ES_MAX_AUD_BUF_TIME 1000              // ms
#define AGMP_ES_DEFAULT_STATUS_UPDATE_INTERVAL 200 // ms

#define AGMP_ES_DEFAULT_PIP_MODE FALSE
#define AGMP_ES_DEFAULT_SERIAL_DATA_MODE TRUE

#define GST_CAPS_FEATURE_SECURE_TS "secure:AesEnc"
#define GST_CAPS_FEATURE_MEMORY_SECMEM_MEMORY "memory:SecMem"
#define GST_CAPS_FEATURE_MEMORY_DMABUF "memory:DMABuf"

static GstClockTime _start_time = 0;
#define GST_TRACER_TS GST_CLOCK_DIFF(_start_time, gst_util_get_timestamp())

#define AGMP_ASSERT_FAIL_DO(expr, fail_do, message) \
    G_STMT_START                                    \
    {                                               \
        if (G_LIKELY(expr))                         \
        {                                           \
        }                                           \
        else                                        \
        {                                           \
            GST_ERROR(message);                     \
            {                                       \
                fail_do;                            \
            }                                       \
        }                                           \
    }                                               \
    G_STMT_END
#define AGMP_ASSERT_FAIL_RET(expr, val, message) AGMP_ASSERT_FAIL_DO(expr, return val, message)
#define AGMP_ASSERT_FAIL_GOTO(expr, go_to, message) AGMP_ASSERT_FAIL_DO(expr, goto go_to, message)

typedef struct _AgmpEsCtxt AgmpEsCtxt;
typedef struct _AgmpEsVidPath AgmpEsVidPath;
typedef struct _AgmpEsAudPath AgmpEsAudPath;
typedef struct _AgmpEsDataControl AgmpEsDataControl;
typedef struct _AgmpMonitorData AgmpMonitorData;
typedef struct _AgmpMsgString AgmpMsgString;
typedef enum _AgmpEsMonitorType AgmpEsMonitorType;
typedef enum _AgmpEsDataStatus AgmpEsDataStatus;

enum _AgmpEsMonitorType
{
    AGMP_ES_MONITOR_STATUS, // monitor msg thread status and do periodic reporting
    AGMP_ES_MONITOR_SECMEM, // monitor secure memory status
};

enum _AgmpEsDataStatus
{
    AGMP_ES_DATA_NEED_BUFFERING,
    AGMP_ES_DATA_NEED_NORMAL,
    AGMP_ES_DATA_BUFFERING_DONE,
};

struct _AgmpEsVidPath
{
    gboolean exist;

    /* cfgs for video path */
    AgmpEsVidCfg cfgs;

    /* secure */
    GstAllocator *sec_allocator;

    /* caps for video path */
    AgmpVidFormatInfo info;
    GstCaps *caps;

    /* elements for video path */
    GstElement *src;
    GstElement *parser;
    GstElement *sec_parser;
    GstElement *decoder;
    GstElement *sink;

#if 0
    AgmpEsSecCtxt *sec_ctxt;
#endif

    /* connected sig id */
    gulong underflow_conn_sig_id;

    /* context */
    gboolean src_data_enough; // appsrc-video queue reached high level
    gboolean src_data_eos;    // appsrc-video received eos event
    GstClockTime max_ts;      // max timestamp appsrc-video received after init playback or seek
    guint total_frame_num;
    guint dropped_frame_num;

    gint data_waiting; // used for serial data mode
};

struct _AgmpEsAudPath
{
    gboolean exist;

    /* cfgs for audio path */
    AgmpEsAudCfg cfgs;

    /* caps for audio path */
    AgmpAudFormatInfo info;
    GstCaps *caps;

    /* elements for audio path */
    GstElement *src;
    GstElement *parser;
    GstElement *decoder;
    GstElement *converter;
    GstElement *resample;
    GstElement *sink;

    /* connected sig id */
    gulong underflow_conn_sig_id;

    /* context */
    gboolean src_data_enough; // appsrc-video queue reached high level
    gboolean src_data_eos;    // appsrc-video received eos event
    GstClockTime max_ts;      // max timestamp appsrc-video received after init playback or seek

    gint data_waiting; // used for serial data mode
};

struct _AgmpEsDataControl
{
    gboolean enable;
    GstClockTime check_interval;
    GstClockTime min_v;
    GstClockTime min_a;
    GstClockTime max_v;
    GstClockTime max_a;
};

struct _AgmpMonitorData
{
    AgmpEsMonitorType monitor_type;
    AgmpEsCtxt *ctxt;
};

struct _AgmpEsCtxt
{
    /* state */
    AgmpEsStateType state;

    /* pipeline context */
    GstElement *pipeline;
    gint bus_watch;
    GMainLoop *main_loop;
    GMainContext *main_loop_context;
    GThread *msg_thread;

    /* data control */
    GThread *data_ctl_thread;
    gboolean quit_data_ctl;
    gboolean paused_internal;
    AgmpEsDataControl data_ctl;

    /* play control from upper-layer */
    gint play_state; // 0 for pause; 1 for play
    double play_rate;
    double play_volume;

    /* cfgs */
    AgmpEsCommonCfg common_cfgs;

    /* A/V path context */
    AgmpEsVidPath v_path;
    AgmpEsAudPath a_path;
    GstAppSrcCallbacks appsrc_cbs;

    /* gsources */
    GSource *player_status_monitor;
    GSource *data_status_monitor;

    /* running flags */
    gboolean wait_preroll;
    gboolean data_pushing_started;
    gboolean flow_force_stop;

    /* seek */
    GstClockTime seek_to_pos;
};

struct _AgmpMsgString
{
    const gint type;
    const gchar *name;
};

static AgmpMsgString messages[] = {
    {AGMP_MSG_STATE_INIT, "state init"},
    {AGMP_MSG_STATE_PREROLL, "state preroll"},
    {AGMP_MSG_STATE_PRESENT, "state present"},
    {AGMP_MSG_STATE_EOS, "state eos"},
    {AGMP_MSG_STATE_DESTROY, "state destroy"},

    {AGMP_MSG_DATA_NEED, "data need"},
    {AGMP_MSG_DATA_ENOUGH, "data enough"},
    {AGMP_MSG_DATA_RELEASE, "data release"},
    {AGMP_MSG_DATA_STAT_LOW, "data status low"},
    {AGMP_MSG_DATA_STAT_HIGH, "data status high"},

    {AGMP_MSG_STATUS_UPDATE, "status update"},

    {AGMP_MSG_ERROR_DEC, "error dec"},
    {AGMP_MSG_ERROR_CAP_CHG, "error cap chg"},

    {0, NULL}};

static gboolean agmp_es_gsource_dispatch(GSource *source, GSourceFunc callback, gpointer usr_data)
{
    gboolean ret;

    GST_TRACE("trace in");

    ret = TRUE;

    if (g_source_get_ready_time(source) == -1)
    {
        ret = G_SOURCE_CONTINUE;
        goto done;
    }
    g_source_set_ready_time(source, -1);

    ret = callback(usr_data);

done:
    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
}

static GSourceFuncs agmp_es_gsource_funcs = {
    NULL,                     // prepare
    NULL,                     // check
    agmp_es_gsource_dispatch, // dispatch
    NULL,                     // finalize
    NULL,                     // closure_callback
    NULL,                     // closure_marshall
};

/* static function declaration */
static AgmpEsCtxt *_agmp_es_init(void);
static void _agmp_es_deinit(AgmpEsCtxt *ctxt);

static void _agmp_es_init_cfgs(AgmpEsCtxt *ctxt);
static void _agmp_es_init_common_cfgs(AgmpEsCommonCfg *common_cfgs);
static void _agmp_es_init_vid_cfgs(AgmpEsVidCfg *vid_cfgs);
static void _agmp_es_init_aud_cfgs(AgmpEsAudCfg *aud_cfgs);

static gboolean _agmp_es_update_cfgs(AgmpEsCtxt *ctxt, AgmpEsCfg *cfgs, gboolean *updated);
static gboolean _agmp_es_update_common_cfgs(AgmpEsCommonCfg *dst, AgmpEsCommonCfg *src, gboolean *updated);
static gboolean _agmp_es_update_vid_cfgs(AgmpEsVidCfg *dst, AgmpEsVidCfg *src, gboolean *updated);
static gboolean _agmp_es_update_aud_cfgs(AgmpEsAudCfg *dst, AgmpEsAudCfg *src, gboolean *updated);

static gboolean _agmp_es_create_paths(AgmpEsCtxt *ctxt);
static gboolean _agmp_es_create_vpath(AgmpEsCtxt *ctxt);
static gboolean _agmp_es_create_apath(AgmpEsCtxt *ctxt);
static gboolean _agmp_es_create_vcaps(AgmpEsCtxt *ctxt);
static gboolean _agmp_es_create_acaps(AgmpEsCtxt *ctxt);

static gboolean _agmp_es_start_msg_thread(AgmpEsCtxt *ctxt);
static gboolean _agmp_es_start_data_ctl_thread(AgmpEsCtxt *ctxt);
static gpointer _agmp_es_msg_thread_func(gpointer data);
static gpointer _agmp_es_data_ctl_thread_func(gpointer data);

static gboolean _agmp_es_bus_cb(GstBus *bus, GstMessage *message, gpointer user_data);

static gboolean _agmp_dispatch_data_msg(AgmpEsCtxt *ctxt, AgmpMsgType msg_type, AgmpEsType es_type, void *data_ptr);
static gboolean _agmp_dispatch_state_msg(AgmpEsCtxt *ctxt, AgmpMsgType msg_type);
static gboolean _agmp_dispatch_status_msg(AgmpEsCtxt *ctxt, AgmpMsgType msg_type);
static gboolean _agmp_dispatch_error_msg(AgmpEsCtxt *ctxt, AgmpMsgType msg_type);
static gboolean _agmp_dispatch_msg_uncheck(AgmpEsCtxt *ctxt, AgmpMsgType msg_type);
static gboolean _agmp_dispatch_msg_on_mainloop(AgmpEsCtxt *ctxt, AgmpMsg *msg);

static gboolean _agmp_es_gsource_cb(gpointer msg);

static gboolean _agmp_es_set_state(AgmpEsCtxt *ctxt, AgmpEsStateType state);
static gboolean _agmp_es_set_pipeline_state(AgmpEsCtxt *ctxt, GstState state);

static gboolean _agmp_es_write_v(AgmpEsCtxt *ctxt, AgmpDataInfo *data_info);
static gboolean _agmp_es_write_a(AgmpEsCtxt *ctxt, AgmpDataInfo *data_info);
static void _agmp_es_free_data_info(AgmpEsCtxt *ctxt, AgmpDataInfo *data_info);

static GstBuffer *_agmp_es_create_buf(AgmpEsCtxt *ctxt, AgmpDataInfo *data_info);
static GstBuffer *_agmp_es_create_key(AgmpEsCtxt *ctxt, AgmpDrmDataInfo *drm_info);
static GstBuffer *_agmp_es_create_iv(AgmpEsCtxt *ctxt, AgmpDrmDataInfo *drm_info);
static GstBuffer *_agmp_es_create_subsamples(AgmpEsCtxt *ctxt, AgmpDrmDataInfo *drm_info);
static gboolean _agmp_es_decrypt(AgmpEsCtxt *ctxt, GstBuffer **buf);
static gboolean _agmp_es_decrypt_external(AgmpEsCtxt *ctxt, GstBuffer **buf);
static GstBuffer *_agmp_es_extract_buf_from_struct(GstStructure *s, const gchar *name);

static void _agmp_es_appsrc_need_data(GstAppSrc *src, guint length, gpointer user_data);
static void _agmp_es_appsrc_enough_data(GstAppSrc *src, gpointer user_data);
static gboolean _agmp_es_appsrc_seek_data(GstAppSrc *src, guint64 offset, gpointer user_data);

#if 0
static void _agmp_es_underflow_cb(GstElement *object, guint arg0, gpointer arg1, gpointer user_data);
#endif

static gboolean _agmp_es_add_monitor(AgmpEsCtxt *ctxt, AgmpEsMonitorType monitor_type);
static void _agmp_es_remove_monitor(AgmpEsCtxt *ctxt, AgmpEsMonitorType monitor_type);
static gboolean _agmp_es_monitor_cb(AgmpMonitorData *monitor_data);

static AgmpEsDataStatus _agmp_es_data_status(AgmpEsCtxt *ctxt, AgmpEsType type);
static void _agmp_es_data_clear_status(AgmpEsCtxt *ctxt);

static GstClockTime _agmp_es_get_position(AgmpEsCtxt *ctxt);

static inline AgmpEsType _agmp_es_appsrc_media_type(AgmpEsCtxt *ctxt, GstAppSrc *src);
static inline gboolean _agmp_es_data_has_enough(AgmpEsCtxt *ctxt);
static inline gboolean _agmp_es_data_all_enough(AgmpEsCtxt *ctxt);
static inline gboolean _agmp_es_data_v_enough(AgmpEsCtxt *ctxt);
static inline gboolean _agmp_es_data_a_enough(AgmpEsCtxt *ctxt);
static inline gboolean _agmp_es_data_has_eos(AgmpEsCtxt *ctxt);
static inline gboolean _agmp_es_data_all_eos(AgmpEsCtxt *ctxt);
static inline gboolean _agmp_es_data_v_eos(AgmpEsCtxt *ctxt);
static inline gboolean _agmp_es_data_a_eos(AgmpEsCtxt *ctxt);
static inline gboolean _agmp_es_is_data_msg(AgmpMsgType type);
static inline gboolean _agmp_es_is_state_msg(AgmpMsgType type);

/* global function definition */
AGMP_ES_HANDLE agmp_es_create(AgmpEsCfg *cfg)
{
    AgmpEsCtxt *ctxt;

    ctxt = NULL;

    AGMP_ASSERT_FAIL_GOTO(cfg, errors, "invalid input cfgs.");
    AGMP_ASSERT_FAIL_GOTO((ctxt = _agmp_es_init()), errors, "init failed.");
    AGMP_ASSERT_FAIL_GOTO(_agmp_es_update_cfgs(ctxt, cfg, NULL), errors, "update cfgs failed.");
    AGMP_ASSERT_FAIL_GOTO(_agmp_es_create_paths(ctxt), errors, "create paths failed.");
    AGMP_ASSERT_FAIL_GOTO(_agmp_es_set_pipeline_state(ctxt, GST_STATE_READY), errors, "chg pipeline state failed."); // change pip state in main thread not in msg thread
    AGMP_ASSERT_FAIL_GOTO(_agmp_es_start_msg_thread(ctxt), errors, "start msg thread failed.");
    AGMP_ASSERT_FAIL_GOTO(_agmp_es_start_data_ctl_thread(ctxt), errors, "start msg thread failed.");
    AGMP_ASSERT_FAIL_GOTO(_agmp_es_add_monitor(ctxt, AGMP_ES_MONITOR_STATUS), errors, "create status monitor failed.");

done:
    return ctxt;

errors:
    _agmp_es_deinit(ctxt);
    ctxt = NULL;
    goto done;
}

void agmp_es_destroy(AGMP_ES_HANDLE handle)
{
    AgmpEsCtxt *ctxt;

    GST_TRACE("trace in");

    ctxt = (AgmpEsCtxt *)handle;

    _agmp_es_set_state(ctxt, AGMP_ES_STATE_DESTROY);

    _agmp_es_deinit(ctxt);

    GST_TRACE("trace out ret void");
}

BOOL agmp_es_acquire_cfgs(AGMP_ES_HANDLE handle, AgmpEsCfg *cfg)
{
    /*
        don't print gst log in this func
        because it called before debug category obj init
    */

    AGMP_ASSERT_FAIL_RET(cfg, FALSE, "invalid input cfgs");

    if (handle)
    {
        AgmpEsCtxt *ctxt;

        ctxt = (AgmpEsCtxt *)handle;

        memcpy(&cfg->common_cfgs, &ctxt->common_cfgs, sizeof(cfg->common_cfgs));
        memcpy(&cfg->vid_cfgs, &ctxt->v_path.cfgs, sizeof(cfg->vid_cfgs));
        memcpy(&cfg->aud_cfgs, &ctxt->a_path.cfgs, sizeof(cfg->aud_cfgs));
        GST_DEBUG("acquired cfgs from agmp-es:%p", handle);
    }
    else
    {
        _agmp_es_init_common_cfgs(&cfg->common_cfgs);
        _agmp_es_init_vid_cfgs(&cfg->vid_cfgs);
        _agmp_es_init_aud_cfgs(&cfg->aud_cfgs);
    }

    return TRUE;
}

BOOL agmp_es_start(AGMP_ES_HANDLE handle)
{
    AgmpEsCtxt *ctxt;
    gboolean ret;

    GST_TRACE("trace in");

    ctxt = (AgmpEsCtxt *)handle;
    ret = TRUE;

    AGMP_ASSERT_FAIL_RET(AGMP_ES_STATE_INIT == ctxt->state, FALSE, "should call this interface in state:AGMP_ES_STATE_INIT");

    ctxt->seek_to_pos = 0; // play from 0 by default

    ret &= _agmp_es_set_state(ctxt, AGMP_ES_STATE_PREROLL_INIT);

    ret &= _agmp_es_set_pipeline_state(ctxt, GST_STATE_PAUSED);

    if (GST_STATE_PAUSED == GST_STATE(ctxt->pipeline) && !ctxt->wait_preroll)
    {
        GST_DEBUG("pipeline has been synchronously switched to the pause state");
        ret &= _agmp_es_set_state(ctxt, AGMP_ES_STATE_PRESENT);
    }

    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
}

BOOL agmp_es_stop(AGMP_ES_HANDLE handle)
{
    AgmpEsCtxt *ctxt;
    GstStructure *structure;
    gboolean ret;

    GST_TRACE("trace in");

    ctxt = (AgmpEsCtxt *)handle;
    ret = TRUE;

    if (ctxt->pipeline)
    {
        GST_INFO("stop pipeline by sending force-stop msg");
        gst_object_ref(ctxt->pipeline);
        structure = gst_structure_new_empty("force-stop");
        gst_element_post_message(ctxt->pipeline, gst_message_new_application(GST_OBJECT(ctxt->pipeline), structure));
        gst_object_unref(ctxt->pipeline);
        ret = TRUE;
    }
    else
    {
        GST_ERROR("pipeline didn't exist");
        ret = FALSE;
    }

    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
}

BOOL agmp_es_update_format(AGMP_ES_HANDLE handle, AgmpFormatInfo *info)
{
    AgmpEsCtxt *ctxt;
    gboolean updated;
    gboolean ret;

    GST_TRACE("trace in");

    ctxt = (AgmpEsCtxt *)handle;
    ret = TRUE;

    if (AGMP_VID == info->type)
    {
        AgmpEsVidCfg tmp_cfgs;

        memcpy(&tmp_cfgs, &ctxt->v_path.cfgs, sizeof(AgmpEsVidCfg));
        memcpy(&tmp_cfgs.format_info, &info->u.vinfo, sizeof(AgmpVidFormatInfo));
        ret = _agmp_es_update_vid_cfgs(&ctxt->v_path.cfgs, &tmp_cfgs, &updated);

        if (updated)
        {
            AGMP_ASSERT_FAIL_RET(_agmp_es_create_vcaps(ctxt), FALSE, "create new vcaps error");
            gst_app_src_set_caps(GST_APP_SRC(ctxt->v_path.src), ctxt->v_path.caps);
        }
    }
    else if (AGMP_AUD == info->type)
    {
        AgmpEsAudCfg tmp_cfgs;
        memcpy(&tmp_cfgs, &ctxt->a_path.cfgs, sizeof(AgmpEsAudCfg));
        memcpy(&tmp_cfgs.format_info, &info->u.ainfo, sizeof(AgmpAudFormatInfo));

        ret = _agmp_es_update_aud_cfgs(&ctxt->a_path.cfgs, &tmp_cfgs, &updated);

        if (updated)
        {
            AGMP_ASSERT_FAIL_RET(_agmp_es_create_acaps(ctxt), FALSE, "create new acaps error");
            gst_app_src_set_caps(GST_APP_SRC(ctxt->a_path.src), ctxt->a_path.caps);
        }
    }

    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
}

BOOL agmp_es_write(AGMP_ES_HANDLE handle, AgmpDataInfo *data_info)
{
    AgmpEsCtxt *ctxt;
    gboolean ret;

    GST_TRACE("trace in");

    ctxt = (AgmpEsCtxt *)handle;

    if (AGMP_VID == data_info->type)
        ret = _agmp_es_write_v(ctxt, data_info);
    else if (AGMP_AUD == data_info->type)
        ret = _agmp_es_write_a(ctxt, data_info);
    else
    {
        GST_ERROR("meet wrong type");
        ret = FALSE;
    }

    _agmp_es_free_data_info(ctxt, data_info);
    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
}

BOOL agmp_es_seek(AGMP_ES_HANDLE handle, double rate, int64_t pos)
{
    AgmpEsCtxt *ctxt;
    gboolean ret;

    GST_TRACE("trace in");

    ctxt = (AgmpEsCtxt *)handle;
    ret = TRUE;

    GST_INFO("seek to pos %" GST_TIME_FORMAT " with rate:%f(old rate:%f)", GST_TIME_ARGS(pos * GST_MSECOND), rate, ctxt->play_rate);
    ctxt->seek_to_pos = pos * GST_MSECOND;
    if (rate != ctxt->play_rate)
        ctxt->play_rate = rate;

    _agmp_es_data_clear_status(ctxt);

    /* only video need preroll */
    if (ctxt->v_path.exist)
        ctxt->wait_preroll = TRUE;

    if (!gst_element_seek(ctxt->pipeline, ctxt->play_rate, GST_FORMAT_TIME,
                          (GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
                          GST_SEEK_TYPE_SET,
                          ctxt->seek_to_pos,
                          GST_SEEK_TYPE_NONE, 0))
    {
        GST_ERROR("send seek event failed.");
        ret = FALSE;
        goto done;
    }

    GST_INFO("send seek event succ.");
    // ctxt->paused_internal = TRUE;
    _agmp_es_set_state(ctxt, AGMP_ES_STATE_PREROLL_AFTER_SEEK);

done:
    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
}

BOOL agmp_es_set_eos(AGMP_ES_HANDLE handle, AgmpEsType type)
{
    AgmpEsCtxt *ctxt;
    GstElement *src;
    gboolean ret;
    GstFlowReturn result;

    GST_TRACE("trace in");

    ctxt = (AgmpEsCtxt *)handle;
    src = NULL;
    ret = TRUE;
    result = GST_FLOW_OK;

    if (AGMP_VID == type)
    {
        src = ctxt->v_path.src;
        ctxt->v_path.src_data_eos = TRUE;
    }
    else if (AGMP_AUD == type)
    {
        src = ctxt->a_path.src;
        ctxt->a_path.src_data_eos = TRUE;
    }
    AGMP_ASSERT_FAIL_RET(src, FALSE, "can't find stream path for this type");

    result = gst_app_src_end_of_stream(GST_APP_SRC(src));
    ret = (result == GST_FLOW_OK);

    /*
       if audio-only or video-only stream, pipeline only get EOS before receiving any normal data,
        meanwhile pipeline is not reached to PAUSED state, just post EOS event to cobalt browser
   */
    if ((!ctxt->a_path.exist && GST_CLOCK_TIME_NONE == ctxt->v_path.max_ts) ||
        (!ctxt->v_path.exist && GST_CLOCK_TIME_NONE == ctxt->a_path.max_ts))
    {
        if ((GST_STATE(ctxt->pipeline) < GST_STATE_PAUSED))
            ret &= _agmp_es_set_state(ctxt, AGMP_ES_STATE_EOS);
    }

    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
}

int64_t agmp_es_data_get_time_level(AGMP_ES_HANDLE handle, AgmpEsType type)
{
    AgmpEsCtxt *ctxt;
    GstElement *src;
    GstClockTime time;

    GST_TRACE("trace in");

    ctxt = (AgmpEsCtxt *)handle;
    src = NULL;
    time = GST_CLOCK_TIME_NONE;

    if (AGMP_VID == type)
        src = ctxt->v_path.src;
    else if (AGMP_AUD == type)
        src = ctxt->a_path.src;
    if (NULL == src)
    {
        GST_ERROR("can't find stream path for this type:%d", type);
        time = GST_CLOCK_TIME_NONE;
        goto done;
    }
    time = gst_app_src_get_current_level_time(GST_APP_SRC(src));

done:
    GST_TRACE("trace out ret int64:%lld (GstClockTime:%" GST_TIME_FORMAT ")", time, GST_TIME_ARGS(time));
    return time;
}

void agmp_es_set_volume(AGMP_ES_HANDLE handle, double volume)
{
    AgmpEsCtxt *ctxt;

    GST_TRACE("trace in");

    ctxt = (AgmpEsCtxt *)handle;

    if (ctxt->a_path.exist && ctxt->a_path.src)
    {
        double old_volume;
        g_object_get(G_OBJECT(ctxt->a_path.sink), "volume", &old_volume, NULL);
        g_object_set(G_OBJECT(ctxt->a_path.sink), "volume", volume, NULL);
        GST_INFO("Change volume from %f to %f", old_volume, volume);

        ctxt->play_volume = old_volume;
    }
    else
        GST_ERROR("Shouldn't set volume in non-audio case");

    GST_TRACE("trace out ret void");
}

void agmp_es_set_display_window(AGMP_ES_HANDLE handle, AgmpWindow *window)
{
    AgmpEsCtxt *ctxt;

    GST_TRACE("trace in");

    ctxt = (AgmpEsCtxt *)handle;

    if (ctxt->v_path.exist && ctxt->v_path.sink)
    {
        gchar *rect = g_strdup_printf("%d,%d,%d,%d", window->x, window->y, window->w, window->h);
        GST_TRACE("Set Bounds: rect %s", rect);
        g_object_set(ctxt->v_path.sink, "rectangle", rect, NULL);
        free(rect);
    }
    else
        GST_ERROR("Shouldn't set display window in non-video case");

    GST_TRACE("trace out ret void");
}

BOOL agmp_es_set_pause(AGMP_ES_HANDLE handle)
{
    AgmpEsCtxt *ctxt;
    gboolean ret;

    GST_TRACE("trace in");

    ctxt = (AgmpEsCtxt *)handle;
    ret = FALSE;

    if (ctxt->state < AGMP_ES_STATE_PREROLL_INIT)
    {
        GST_ERROR("should not set pause when agmp-es is in state:%d", ctxt->state);
        ret = FALSE;
        goto done;
    }

    ctxt->play_state = 0;
    ret = _agmp_es_set_pipeline_state(ctxt, GST_STATE_PAUSED);

done:
    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
}

BOOL agmp_es_set_play(AGMP_ES_HANDLE handle)
{
    AgmpEsCtxt *ctxt;
    gboolean ret;

    GST_TRACE("trace in");

    ret = TRUE;

    ctxt = (AgmpEsCtxt *)handle;

    ctxt->play_state = 1;

    if (ctxt->paused_internal)
        GST_INFO("ignore upper-layer play command when paused internal");
    else
        ret = _agmp_es_set_pipeline_state(ctxt, GST_STATE_PLAYING);

    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
}

BOOL agmp_es_set_rate(AGMP_ES_HANDLE handle, double rate)
{
    AgmpEsCtxt *ctxt;
    GstSegment *segment;
    gboolean ret;

    GST_TRACE("trace in");

    ctxt = (AgmpEsCtxt *)handle;
    ret = TRUE;

    if (rate == ctxt->play_rate)
    {
        GST_INFO("rate not change");
        ret = TRUE;
        goto done;
    }

    ctxt->play_rate = rate;

    if (ctxt->a_path.exist && ctxt->a_path.src)
    {
        GstPad *sink_pad = gst_element_get_static_pad(ctxt->a_path.sink, "sink");
        if (sink_pad)
        {
            segment = gst_segment_new();
            gst_segment_init(segment, GST_FORMAT_TIME);
            segment->rate = ctxt->play_rate;
            segment->start = GST_CLOCK_TIME_NONE;
            segment->position = GST_CLOCK_TIME_NONE;
            segment->stop = GST_SEEK_TYPE_NONE;
            segment->flags = GST_SEGMENT_FLAG_NONE;
            segment->format = GST_FORMAT_TIME;

            ret = gst_pad_send_event(sink_pad, gst_event_new_segment(segment));
            if (!ret)
                GST_ERROR("Error when sending rate segment!!!\n");
            else
                GST_WARNING("sent segment rate: %f", rate);

            gst_segment_free(segment);
            gst_object_unref(sink_pad);
        }
        else
        {
            ret = FALSE;
            GST_ERROR("no sink pad");
        }
    }
    else
    {
        GST_INFO("cant not get audio sink");
    }

done:
    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
}

AgmpEsStateType agmp_es_get_state(AGMP_ES_HANDLE handle)
{
    AgmpEsCtxt *ctxt;

    GST_TRACE("trace in");

    ctxt = (AgmpEsCtxt *)handle;

    GST_TRACE("trace out ret AgmpEsStateType:%d", ctxt->state);
    return ctxt->state;
}

BOOL agmp_es_get_play_info(AGMP_ES_HANDLE handle, AgmpPlayInfo *play_info)
{
    AgmpEsCtxt *ctxt;
    gboolean ret;

    GST_TRACE("trace in");

    ctxt = (AgmpEsCtxt *)handle;
    ret = TRUE;

    gst_element_query_duration(ctxt->pipeline, GST_FORMAT_TIME, &play_info->duration);
    gst_element_query_position(ctxt->pipeline, GST_FORMAT_TIME, &play_info->position);

    play_info->duration /= GST_MSECOND;
    play_info->position /= GST_MSECOND;

    if (ctxt->v_path.exist)
    {
        play_info->frame_width = ctxt->v_path.cfgs.format_info.frame_width;
        play_info->frame_height = ctxt->v_path.cfgs.format_info.frame_height;
        play_info->total_video_frames = ctxt->v_path.total_frame_num;
        play_info->corrupted_video_frames = 0;
        if (ctxt->v_path.sink)
            g_object_get(G_OBJECT(ctxt->v_path.sink), "frames-dropped", &play_info->dropped_video_frames, NULL);
    }
    else
    {
        play_info->total_video_frames = -1;
        play_info->corrupted_video_frames = -1;
        play_info->frame_width = -1;
        play_info->frame_height = -1;
    }

    play_info->is_paused = GST_STATE(ctxt->pipeline) != GST_STATE_PLAYING;

    play_info->volume = ctxt->play_volume;
    play_info->playback_rate = ctxt->play_rate;

    GST_INFO("play info - [ dur:%lld(ms), pos:%lld(ms), vol:%f, rate:%f, w:%d, h:%d, total:%d, drop:%d, paused:%d ]",
             play_info->duration, play_info->position,
             play_info->volume, play_info->playback_rate,
             play_info->frame_width, play_info->frame_height,
             play_info->total_video_frames, play_info->dropped_video_frames,
             play_info->is_paused);

    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
}

/* static function definition */
AgmpEsCtxt *_agmp_es_init(void)
{
    AgmpEsCtxt *ctxt;
    GstBus *bus;

    ctxt = NULL;
    bus = NULL;

    /* NOTE: There is no need to call gst_init because gst is initialized in main.cc */

    GST_DEBUG_CATEGORY_INIT(agmp_es_debug, "agmp_es_player", 0, "AGMP ES Player");

    GST_TRACE("trace in");

    _start_time = gst_util_get_timestamp();

    AGMP_ASSERT_FAIL_GOTO((ctxt = g_new0(AgmpEsCtxt, 1)), errors, "new AgmpEsCtxt failed.");
    memset(ctxt, 0, sizeof(AgmpEsCtxt));

    AGMP_ASSERT_FAIL_GOTO((ctxt->main_loop_context = g_main_context_new()), errors, "new main loop ctxt failed.");
    g_main_context_push_thread_default(ctxt->main_loop_context);
    AGMP_ASSERT_FAIL_GOTO((ctxt->main_loop = g_main_loop_new(ctxt->main_loop_context, FALSE)), errors, "new main loop failed.");

    GST_DEBUG("watch bus after create mainloop");
    AGMP_ASSERT_FAIL_GOTO((ctxt->pipeline = gst_pipeline_new("agmp_es_static_pipeline")), errors, "new pipeline element failed.");
    AGMP_ASSERT_FAIL_GOTO((bus = gst_element_get_bus(ctxt->pipeline)), errors, "get bus from pipeline failed.");
    ctxt->bus_watch = gst_bus_add_watch(bus, _agmp_es_bus_cb, ctxt);
    gst_object_unref(bus);

    ctxt->paused_internal = TRUE;

    ctxt->appsrc_cbs.need_data = _agmp_es_appsrc_need_data;
    ctxt->appsrc_cbs.enough_data = _agmp_es_appsrc_enough_data;
    ctxt->appsrc_cbs.seek_data = _agmp_es_appsrc_seek_data;

    ctxt->data_ctl.enable = TRUE;
    ctxt->data_ctl.check_interval = AGMP_ES_CHECK_BUF_INTERVAL;
    ctxt->data_ctl.min_v = AGMP_ES_MIN_VID_BUF_TIME;
    ctxt->data_ctl.min_a = AGMP_ES_MIN_AUD_BUF_TIME;
    ctxt->data_ctl.max_v = AGMP_ES_MAX_VID_BUF_TIME;
    ctxt->data_ctl.max_a = AGMP_ES_MAX_AUD_BUF_TIME;

    ctxt->seek_to_pos = GST_CLOCK_TIME_NONE;

    _agmp_es_init_cfgs(ctxt);

done:
    GST_TRACE("trace out ret ptr:%p", ctxt);
    return ctxt;

errors:
    _agmp_es_deinit(ctxt);
    ctxt = NULL;
    goto done;
}

void _agmp_es_deinit(AgmpEsCtxt *ctxt)
{
    GST_TRACE("trace in");

    if (ctxt)
    {
        if (ctxt->data_ctl_thread)
        {
            ctxt->quit_data_ctl = TRUE;
            g_thread_join(ctxt->data_ctl_thread);
        }
        if (ctxt->player_status_monitor)
        {
            g_source_destroy(ctxt->player_status_monitor);
            g_source_unref(ctxt->player_status_monitor);
        }
        if (ctxt->data_status_monitor)
        {
            g_source_destroy(ctxt->data_status_monitor);
            g_source_unref(ctxt->data_status_monitor);
        }

        if (ctxt->pipeline)
        {
            _agmp_es_set_pipeline_state(ctxt, GST_STATE_NULL);
            GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(ctxt->pipeline));
            gst_bus_set_sync_handler(bus, NULL, NULL, NULL);
            gst_object_unref(bus);
            gst_object_unref(ctxt->pipeline);
        }

        if (ctxt->v_path.sec_allocator)
            gst_object_unref(ctxt->v_path.sec_allocator);
        if (ctxt->v_path.caps)
            gst_caps_unref(ctxt->v_path.caps);
        if (ctxt->v_path.parser)
            gst_object_unref(ctxt->v_path.parser);
        if (ctxt->v_path.sec_parser)
            gst_object_unref(ctxt->v_path.parser);
        if (ctxt->v_path.decoder)
            gst_object_unref(ctxt->v_path.decoder);
        if (ctxt->v_path.sink)
        {
            if (ctxt->v_path.underflow_conn_sig_id)
                g_signal_handler_disconnect(ctxt->v_path.sink, ctxt->v_path.underflow_conn_sig_id);
            gst_object_unref(ctxt->v_path.sink);
        }
#if 0
        if (ctxt->v_path.sec_ctxt)
            agmp_es_sec_destroy(ctxt->v_path.sec_ctxt);
#endif

        if (ctxt->a_path.caps)
            gst_caps_unref(ctxt->a_path.caps);
        if (ctxt->a_path.parser)
            gst_object_unref(ctxt->a_path.parser);
        if (ctxt->a_path.decoder)
            gst_object_unref(ctxt->a_path.decoder);
        if (ctxt->a_path.converter)
            gst_object_unref(ctxt->a_path.converter);
        if (ctxt->a_path.resample)
            gst_object_unref(ctxt->a_path.resample);
        if (ctxt->a_path.sink)
        {
            if (ctxt->a_path.underflow_conn_sig_id)
                g_signal_handler_disconnect(ctxt->a_path.sink, ctxt->a_path.underflow_conn_sig_id);
            gst_object_unref(ctxt->a_path.sink);
        }

        if (ctxt->bus_watch > -1)
            g_source_remove(ctxt->bus_watch);
        if (ctxt->msg_thread)
            g_thread_join(ctxt->msg_thread);
        if (ctxt->main_loop_context)
            g_main_context_unref(ctxt->main_loop_context);
        if (ctxt->main_loop)
            g_main_loop_unref(ctxt->main_loop);

        g_free(ctxt);
    }

    GST_TRACE("trace out ret void");
    return;
}

void _agmp_es_init_cfgs(AgmpEsCtxt *ctxt)
{
    GST_TRACE("trace in");

    _agmp_es_init_common_cfgs(&ctxt->common_cfgs);
    _agmp_es_init_vid_cfgs(&ctxt->v_path.cfgs);
    _agmp_es_init_aud_cfgs(&ctxt->a_path.cfgs);

    GST_TRACE("trace out ret void");
}

void _agmp_es_init_common_cfgs(AgmpEsCommonCfg *common_cfgs)
{
    /*
        don't print gst log in this func
        because it called before debug category obj init
    */

    common_cfgs->pip_mode = AGMP_ES_DEFAULT_PIP_MODE;
    common_cfgs->secure_mode = FALSE;
    common_cfgs->serial_data_mode = AGMP_ES_DEFAULT_SERIAL_DATA_MODE;
    common_cfgs->status_update_interval = AGMP_ES_DEFAULT_STATUS_UPDATE_INTERVAL;
    common_cfgs->msg_cb = NULL;
    common_cfgs->user_data = NULL;
}

void _agmp_es_init_vid_cfgs(AgmpEsVidCfg *vid_cfgs)
{
    /*
        don't print gst log in this func
        because it called before debug category obj init
    */

    vid_cfgs->vcodec = VCODEC_NONE;
    vid_cfgs->src_max_byte_size = AGMP_ES_DEFAULT_VID_SRC_SIZE;
    vid_cfgs->src_min_percent = AGMP_ES_DEFAULT_VID_SRC_MIN_PERCENT;
    vid_cfgs->src_low_percent = AGMP_ES_DEFAULT_VID_SRC_LOW_PERCENT;
    vid_cfgs->disp_window.x = 0;
    vid_cfgs->disp_window.y = 0;
    vid_cfgs->disp_window.w = 1920;
    vid_cfgs->disp_window.h = 1080;
    vid_cfgs->low_mem_mode = FALSE;
    vid_cfgs->format_info.frame_width = -1;
    vid_cfgs->format_info.frame_height = -1;
    vid_cfgs->format_info.has_color_metadata = FALSE;
    memset(&vid_cfgs->format_info.color_metadata, 0, sizeof(AgmpMediaColorMetadata));
}

void _agmp_es_init_aud_cfgs(AgmpEsAudCfg *aud_cfgs)
{
    /*
        don't print gst log in this func
        because it called before debug category obj init
    */

    aud_cfgs->acodec = ACODEC_NONE;
    aud_cfgs->src_max_byte_size = AGMP_ES_DEFAULT_AUD_SRC_SIZE;
    aud_cfgs->src_min_percent = AGMP_ES_DEFAULT_AUD_SRC_MIN_PERCENT;
    aud_cfgs->src_low_percent = AGMP_ES_DEFAULT_AUD_SRC_LOW_PERCENT;
    aud_cfgs->format_info.number_of_channels = 0;
    aud_cfgs->format_info.samples_per_second = 0;
    memset(&aud_cfgs->format_info.data.data, 0, AGMP_ES_AUD_SPEC_DATA_MAX_SIZE);
    aud_cfgs->format_info.data.size = 0;
}

gboolean _agmp_es_update_cfgs(AgmpEsCtxt *ctxt, AgmpEsCfg *cfgs, gboolean *updated_in)
{
    gboolean ret, ret_common, ret_vid, ret_aud;
    gboolean updated, updated_common, updated_vid, updated_aud;

    GST_TRACE("trace in");

    ret = ret_common = ret_vid = ret_aud = FALSE;
    updated = updated_common = updated_vid = updated_aud = FALSE;

    ret_common = _agmp_es_update_common_cfgs(&ctxt->common_cfgs, &cfgs->common_cfgs, &updated_common);
    GST_DEBUG("common cfgs update result:%d, updated:%d", ret_common, updated_common);
    ret_vid = _agmp_es_update_vid_cfgs(&ctxt->v_path.cfgs, &cfgs->vid_cfgs, &updated_vid);
    GST_DEBUG("vid    cfgs update result:%d, updated:%d", ret_vid, updated_vid);
    ret_aud = _agmp_es_update_aud_cfgs(&ctxt->a_path.cfgs, &cfgs->aud_cfgs, &updated_aud);
    GST_DEBUG("aud    cfgs update result:%d, updated:%d", ret_aud, updated_aud);

    updated = updated_common || updated_vid || updated_aud;
    ret = ret_common && ret_vid && ret_aud;
    GST_DEBUG("updated:%d, ret:%d", updated, ret);

    if (updated_in)
        *updated_in = updated;

    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
}

gboolean _agmp_es_update_common_cfgs(AgmpEsCommonCfg *dst, AgmpEsCommonCfg *src, gboolean *updated)
{
    gboolean is_updated;
    gboolean ret;

    GST_TRACE("trace in");

    is_updated = FALSE;
    ret = TRUE;

    if (!memcmp(dst, src, sizeof(AgmpEsCommonCfg)))
    {
        GST_DEBUG("common cfgs not change");
        goto done;
    }

    dst->pip_mode = src->pip_mode;
    dst->secure_mode = src->secure_mode;

    if (src->status_update_interval != 0)
        dst->status_update_interval = src->status_update_interval;

    dst->user_data = src->user_data;
    dst->msg_cb = src->msg_cb;
    dst->decrypt = src->decrypt;

    is_updated = TRUE;

done:
    if (updated)
        *updated = is_updated;

    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
}

gboolean _agmp_es_update_vid_cfgs(AgmpEsVidCfg *dst, AgmpEsVidCfg *src, gboolean *updated)
{
    gboolean ret;
    gboolean is_updated;

    GST_TRACE("trace in");

    ret = TRUE;
    is_updated = FALSE;

    if (!memcmp(dst, src, sizeof(AgmpEsVidCfg)))
    {
        GST_DEBUG("video cfgs not change");
        goto done;
    }
    if (VCODEC_NONE == src->vcodec)
    {
        GST_DEBUG("src video cfgs is invalid");
        goto done;
    }
    if (VCODEC_NONE != dst->vcodec && dst->vcodec != src->vcodec)
    {
        GST_ERROR("didn't support codec change for now");
        ret = FALSE;
        goto done;
    }

    dst->vcodec = src->vcodec;

    if (src->src_max_byte_size != -1)
        dst->src_max_byte_size = src->src_max_byte_size;
    if (src->src_min_percent != -1)
        dst->src_min_percent = src->src_min_percent;

    if ((src->disp_window.x > 0 && src->disp_window.x < 3840) &&
        (src->disp_window.y > 0 && src->disp_window.x < 2160) &&
        (src->disp_window.w > 0 && src->disp_window.w < 3840) &&
        (src->disp_window.h > 0 && src->disp_window.h < 2160))
        memcpy(&dst->disp_window, &src->disp_window, sizeof(AgmpWindow));

    dst->low_mem_mode = src->low_mem_mode;

    if (src->format_info.frame_width != 0 && src->format_info.frame_width != -1 &&
        src->format_info.frame_height != 0 && src->format_info.frame_height != -1)
    {
        dst->format_info.frame_width = src->format_info.frame_width;
        dst->format_info.frame_height = src->format_info.frame_height;
    }

    if (src->format_info.has_color_metadata)
    {
        dst->format_info.has_color_metadata = TRUE;
        memcpy(&dst->format_info.color_metadata, &src->format_info.color_metadata, sizeof(AgmpMediaColorMetadata));
    }

    is_updated = TRUE;

done:
    if (updated)
        *updated = is_updated;

    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
}

gboolean _agmp_es_update_aud_cfgs(AgmpEsAudCfg *dst, AgmpEsAudCfg *src, gboolean *updated)
{
    gboolean ret;
    gboolean is_updated;

    GST_TRACE("trace in");

    ret = TRUE;
    is_updated = FALSE;

    if (!memcmp(dst, src, sizeof(AgmpEsAudCfg)))
    {
        GST_DEBUG("audio cfgs not change");
        goto done;
    }

    if (ACODEC_NONE == src->acodec)
    {
        GST_DEBUG("src audio cfgs is invalid");
        goto done;
    }
    if (ACODEC_NONE != dst->acodec && dst->acodec != src->acodec)
    {
        GST_ERROR("didn't support codec change for now");
        ret = FALSE;
        goto done;
    }

    dst->acodec = src->acodec;

    if (src->src_max_byte_size != -1)
        dst->src_max_byte_size = src->src_max_byte_size;
    if (src->src_min_percent != -1)
        dst->src_min_percent = src->src_min_percent;

    if (src->format_info.number_of_channels != 0 && src->format_info.number_of_channels != -1)
        dst->format_info.number_of_channels = src->format_info.number_of_channels;

    if (src->format_info.samples_per_second != 0 && src->format_info.samples_per_second != -1)
        dst->format_info.samples_per_second = src->format_info.samples_per_second;

    if (src->format_info.data.size > 19)
    {
        GST_ERROR("audio spec data size shouldn;t above 19");
        ret = FALSE;
        goto done;
    }

    if (src->format_info.data.data && src->format_info.data.size > 0)
    {
        memcpy(&dst->format_info.data.data, &src->format_info.data.data, src->format_info.data.size);
        dst->format_info.data.size = src->format_info.data.size;
    }

    is_updated = TRUE;

done:
    if (updated)
        *updated = is_updated;

    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
}

gboolean _agmp_es_create_paths(AgmpEsCtxt *ctxt)
{
    gboolean ret, ret_vpath, ret_apath;

    GST_TRACE("trace in");

    ret = ret_vpath = ret_apath = FALSE;

    ret_vpath = _agmp_es_create_vpath(ctxt);
    GST_DEBUG("create vpath:%d", ret_vpath);
    ret_apath = _agmp_es_create_apath(ctxt);
    GST_DEBUG("create apath:%d", ret_apath);

    ret = ret_vpath && ret_apath;

    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
}

gboolean _agmp_es_create_vpath(AgmpEsCtxt *ctxt)
{
    AgmpVidCodecType codec;
    GstElement *tmp;
    gboolean ret;

    GST_TRACE("trace in");

    codec = ctxt->v_path.cfgs.vcodec;
    ret = TRUE;

    if (VCODEC_NONE == codec)
    {
        GST_DEBUG("The current playback does not contain video");
        ctxt->v_path.exist = FALSE;
        ret = TRUE;
        goto done;
    }

    /* create secure allocator for secure case */
    // TODO:need to deal with secure_mode is TRUE but only audio is enc
    if (ctxt->common_cfgs.secure_mode)
    {
        uint8_t format = SECMEM_DECODER_DEFAULT;
        if (VCODEC_AV1 == codec)
            format = SECMEM_DECODER_AV1;
        else if (VCODEC_VP9 == codec)
            format = SECMEM_DECODER_VP9;

        gboolean is_4k = TRUE;
        if (ctxt->v_path.cfgs.disp_window.w <= 1920 && ctxt->v_path.cfgs.disp_window.h <= 1080)
        {
            is_4k = FALSE;
        }
        AGMP_ASSERT_FAIL_GOTO((ctxt->v_path.sec_allocator = gst_secmem_allocator_new(is_4k, format)), errors, "create secure allocator meet error.");
    }

    /* init vpath ctxt flags */
    ctxt->v_path.exist = TRUE;
    ctxt->v_path.src_data_enough = FALSE;
    ctxt->v_path.src_data_eos = FALSE;
    ctxt->v_path.max_ts = GST_CLOCK_TIME_NONE;
    ctxt->v_path.total_frame_num = 0;
    ctxt->v_path.dropped_frame_num = 0;
    g_atomic_int_set(&ctxt->v_path.data_waiting, 0);

    /* create video caps */
    AGMP_ASSERT_FAIL_GOTO(_agmp_es_create_vcaps(ctxt), errors, "create video caps failed.");

    /* make parser & decoder elements */
    switch (codec)
    {
    case VCODEC_H264:
    {
        if (ctxt->common_cfgs.secure_mode)
            ctxt->v_path.sec_parser = gst_element_factory_make("h264secparse", "h264secparse");
        else
            ctxt->v_path.parser = gst_element_factory_make("h264parse", "h264parse");

        ctxt->v_path.decoder = gst_element_factory_make("amlv4l2h264dec", "amlv4l2h264dec");

        if ((!ctxt->common_cfgs.secure_mode && !ctxt->v_path.parser) ||
            (ctxt->common_cfgs.secure_mode && !ctxt->v_path.sec_parser) ||
            !ctxt->v_path.decoder)
            goto errors;

        break;
    }
    case VCODEC_H265:
    {
        if (ctxt->common_cfgs.secure_mode)
            ctxt->v_path.sec_parser = gst_element_factory_make("h265secparse", "h265secparse");
        else
            ctxt->v_path.parser = gst_element_factory_make("h265parse", "h265parse");

        ctxt->v_path.decoder = gst_element_factory_make("amlv4l2h265dec", "amlv4l2h265dec");

        if (!ctxt->v_path.parser || !ctxt->v_path.decoder)
            goto errors;

        break;
    }
    case VCODEC_MPEG2:
    {
        ctxt->v_path.decoder = gst_element_factory_make("amlv4l2mpeg4dec", "amlv4l2mpeg4dec");

        if (!ctxt->v_path.decoder)
            goto errors;

        break;
    }
    case VCODEC_THEORA:
    {
        // TODO: need add S/W decoder?
        goto errors;
    }
    case VCODEC_VC1:
    {
        ctxt->v_path.decoder = gst_element_factory_make("amlv4l2vc1dec", "amlv4l2vc1dec");

        if (!ctxt->v_path.decoder)
            goto errors;

        break;
    }
    case VCODEC_AV1:
    {
        ctxt->v_path.decoder = gst_element_factory_make("amlv4l2av1dec", "amlv4l2av1dec");

        if (!ctxt->v_path.decoder)
            goto errors;

        break;
    }
    case VCODEC_VP8:
    {
        // TODO: need add S/W decoder?
        goto errors;
    }
    case VCODEC_VP9:
    {
        ctxt->v_path.decoder = gst_element_factory_make("amlv4l2vp9dec", "amlv4l2vp9dec");

        if (!ctxt->v_path.decoder)
            goto errors;

        break;
    }
    default:
    {
        GST_ERROR("meet unknown codec type");
        goto errors;
    }
    }

    /* make src element */
    AGMP_ASSERT_FAIL_GOTO((ctxt->v_path.src = gst_element_factory_make("appsrc", "vidsrc")), errors, "create video appsrc failed.");
    gst_app_src_set_caps(GST_APP_SRC(ctxt->v_path.src), ctxt->v_path.caps);
    g_object_set(ctxt->v_path.src, "block", FALSE, "format", GST_FORMAT_TIME, "stream-type", GST_APP_STREAM_TYPE_SEEKABLE, NULL);
    gst_app_src_set_callbacks(GST_APP_SRC(ctxt->v_path.src), &ctxt->appsrc_cbs, ctxt, NULL);
    gst_app_src_set_max_bytes(GST_APP_SRC(ctxt->v_path.src), ctxt->v_path.cfgs.src_max_byte_size);
    g_object_set(G_OBJECT(ctxt->v_path.src), "min-percent", ctxt->v_path.cfgs.src_min_percent, NULL);
    GST_DEBUG("cfg vid-appsrc max bytes:%d, min percent:%d", ctxt->v_path.cfgs.src_max_byte_size, ctxt->v_path.cfgs.src_min_percent);

    /* make sink element */
    AGMP_ASSERT_FAIL_GOTO((ctxt->v_path.sink = gst_element_factory_make("amlvideosink", "vidsink")), errors, "create video sink failed.");
    // if (ctxt->common_cfgs.pip_mode)
    //     g_object_set(G_OBJECT(ctxt->v_path.sink), "pip", TRUE, NULL);
    // // TODO:check amlvideosink support low-memory property
    // if (ctxt->v_path.cfgs.low_mem_mode)
    //     g_object_set(G_OBJECT(ctxt->v_path.sink), "low-memory", TRUE, NULL);
    // // TODO:need check underflow signal name in amlvideosink
    // ctxt->v_path.underflow_conn_sig_id = g_signal_connect_swapped(ctxt->v_path.sink,
    //                                                               "buffer-underflow-callback",
    //                                                               G_CALLBACK(_agmp_es_underflow_cb), ctxt);

    /* add elements */
    tmp = NULL;
    if (ctxt->v_path.src)
    {
        gst_bin_add(GST_BIN(ctxt->pipeline), ctxt->v_path.src);
        tmp = ctxt->v_path.src;
    }
    if (ctxt->v_path.parser)
    {
        gst_bin_add(GST_BIN(ctxt->pipeline), ctxt->v_path.parser);
        AGMP_ASSERT_FAIL_GOTO(tmp, errors, "link failed.");
        gst_element_link(tmp, ctxt->v_path.parser);
        tmp = ctxt->v_path.parser;
    }
    if (ctxt->v_path.sec_parser)
    {
        gst_bin_add(GST_BIN(ctxt->pipeline), ctxt->v_path.sec_parser);
        AGMP_ASSERT_FAIL_GOTO(tmp, errors, "link failed.");
        gst_element_link(tmp, ctxt->v_path.sec_parser);
        tmp = ctxt->v_path.sec_parser;
    }
    if (ctxt->v_path.decoder)
    {
        gst_bin_add(GST_BIN(ctxt->pipeline), ctxt->v_path.decoder);
        AGMP_ASSERT_FAIL_GOTO(tmp, errors, "link failed.");
        gst_element_link(tmp, ctxt->v_path.decoder);
        tmp = ctxt->v_path.decoder;
    }
    if (ctxt->v_path.sink)
    {
        gst_bin_add(GST_BIN(ctxt->pipeline), ctxt->v_path.sink);
        AGMP_ASSERT_FAIL_GOTO(tmp, errors, "link failed.");
        gst_element_link(tmp, ctxt->v_path.sink);
    }

    ret = TRUE;

done:
    GST_TRACE("trace out ret bool:%d", ret);
    return ret;

errors:
    GST_ERROR("create caps or create elements or link elements meet error");
    _agmp_es_deinit(ctxt);
    ret = FALSE;
    goto done;
}

gboolean _agmp_es_create_apath(AgmpEsCtxt *ctxt)
{
    AgmpAudCodecType codec;
    GstElement *tmp;
    gboolean ret;

    GST_TRACE("trace in");

    codec = ctxt->a_path.cfgs.acodec;
    ret = TRUE;

    if (ACODEC_NONE == codec)
    {
        GST_DEBUG("The current playback does not contain audio");
        ctxt->a_path.exist = FALSE;
        ret = TRUE;
        goto done;
    }

    /* init vpath ctxt flags */
    ctxt->a_path.exist = TRUE;
    ctxt->a_path.src_data_enough = FALSE;
    ctxt->a_path.src_data_eos = FALSE;
    ctxt->a_path.max_ts = GST_CLOCK_TIME_NONE;
    g_atomic_int_set(&ctxt->a_path.data_waiting, 0);

    /* create audio caps */
    AGMP_ASSERT_FAIL_GOTO(_agmp_es_create_acaps(ctxt), errors, "create audio caps failed.");

    /* make parser & decoder & converter & resample elements */
    switch (codec)
    {
    case ACODEC_AAC:
    {
        ctxt->a_path.parser = gst_element_factory_make("aacparse", "aacparse");
        ctxt->a_path.decoder = gst_element_factory_make("avdec_aac", "avdec_aac");

        if (!ctxt->a_path.parser || !ctxt->a_path.decoder)
            goto errors;

        break;
    }
    case ACODEC_AC3:
    case ACODEC_EAC3:
    {
        ctxt->a_path.parser = gst_element_factory_make("ac3parse", "ac3parse");

        if (!ctxt->a_path.parser)
            goto errors;

        break;
    }
    case ACODEC_OPUS:
    {
        ctxt->a_path.decoder = gst_element_factory_make("opusdec", "opusdec");

        if (!ctxt->a_path.decoder)
            goto errors;

        break;
    }
    case ACODEC_VORBIS:
    {
        ctxt->a_path.parser = gst_element_factory_make("vorbisparse", "vorbisparse");
        ctxt->a_path.decoder = gst_element_factory_make("vorbisdec", "vorbisdec");

        if (!ctxt->a_path.parser || !ctxt->a_path.decoder)
            goto errors;

        break;
    }
    case ACODEC_MP3:
    case ACODEC_FLAC:
    case ACODEC_PCM:
    {
        // TODO: add audio codecs
        goto errors;
    }
    default:
    {
        GST_ERROR("meet unknown codec type");
        goto errors;
    }
    }

    if (codec != ACODEC_AC3 && codec != ACODEC_EAC3)
    {
        ctxt->a_path.converter = gst_element_factory_make("audioconvert", "audioconvert");
        ctxt->a_path.resample = gst_element_factory_make("audioresample", "audioresample");

        if (!ctxt->a_path.converter || !ctxt->a_path.resample)
            goto errors;
    }

    /* make src element */
    AGMP_ASSERT_FAIL_GOTO((ctxt->a_path.src = gst_element_factory_make("appsrc", "audsrc")), errors, "create audio appsrc failed.");
    gst_app_src_set_caps(GST_APP_SRC(ctxt->a_path.src), ctxt->a_path.caps);
    g_object_set(ctxt->a_path.src, "block", FALSE, "format", GST_FORMAT_TIME, "stream-type", GST_APP_STREAM_TYPE_SEEKABLE, NULL);
    gst_app_src_set_callbacks(GST_APP_SRC(ctxt->a_path.src), &ctxt->appsrc_cbs, ctxt, NULL);
    gst_app_src_set_max_bytes(GST_APP_SRC(ctxt->a_path.src), ctxt->a_path.cfgs.src_max_byte_size);
    g_object_set(G_OBJECT(ctxt->a_path.src), "min-percent", ctxt->a_path.cfgs.src_min_percent, NULL);
    GST_DEBUG("cfg aud-appsrc max bytes:%d, min percent:%d", ctxt->a_path.cfgs.src_max_byte_size, ctxt->a_path.cfgs.src_min_percent);

    /* make sink element */
    AGMP_ASSERT_FAIL_GOTO((ctxt->a_path.sink = gst_element_factory_make("amlhalasink", "audsink")), errors, "create audio sink failed.");
    g_object_set(G_OBJECT(ctxt->a_path.sink), "wait-video", TRUE, NULL);
    g_object_set(G_OBJECT(ctxt->a_path.sink), "a-wait-timeout", 4000, NULL);
    g_object_set(G_OBJECT(ctxt->a_path.sink), "disable-xrun", FALSE, NULL);
    if (ctxt->common_cfgs.pip_mode)
        g_object_set(G_OBJECT(ctxt->a_path.sink), "direct-mode", FALSE, NULL);
    // ctxt->a_path.underflow_conn_sig_id = g_signal_connect_swapped(ctxt->a_path.sink,
    //                                                               "underrun-callback",
    //                                                               G_CALLBACK(_agmp_es_underflow_cb), ctxt);
    /* add elements */
    tmp = NULL;
    if (ctxt->a_path.src)
    {
        gst_bin_add(GST_BIN(ctxt->pipeline), ctxt->a_path.src);
        tmp = ctxt->a_path.src;
    }
    if (ctxt->a_path.parser)
    {
        gst_bin_add(GST_BIN(ctxt->pipeline), ctxt->a_path.parser);
        AGMP_ASSERT_FAIL_GOTO(tmp, errors, "link failed.");
        gst_element_link(tmp, ctxt->a_path.parser);
        tmp = ctxt->a_path.parser;
    }
    if (ctxt->a_path.decoder)
    {
        gst_bin_add(GST_BIN(ctxt->pipeline), ctxt->a_path.decoder);
        AGMP_ASSERT_FAIL_GOTO(tmp, errors, "link failed.");
        gst_element_link(tmp, ctxt->a_path.decoder);
        tmp = ctxt->a_path.decoder;
    }
    if (ctxt->a_path.converter)
    {
        gst_bin_add(GST_BIN(ctxt->pipeline), ctxt->a_path.converter);
        AGMP_ASSERT_FAIL_GOTO(tmp, errors, "link failed.");
        gst_element_link(tmp, ctxt->a_path.converter);
        tmp = ctxt->a_path.converter;
    }
    if (ctxt->a_path.resample)
    {
        gst_bin_add(GST_BIN(ctxt->pipeline), ctxt->a_path.resample);
        AGMP_ASSERT_FAIL_GOTO(tmp, errors, "link failed.");
        gst_element_link(tmp, ctxt->a_path.resample);
        tmp = ctxt->a_path.resample;
    }
    if (ctxt->a_path.sink)
    {
        gst_bin_add(GST_BIN(ctxt->pipeline), ctxt->a_path.sink);
        AGMP_ASSERT_FAIL_GOTO(tmp, errors, "link failed.");
        gst_element_link(tmp, ctxt->a_path.sink);
        tmp = ctxt->a_path.sink;
    }

    ret = TRUE;

done:
    GST_TRACE("trace out ret bool:%d", ret);
    return ret;

errors:
    GST_ERROR("create caps or create elements or link elements meet error");
    _agmp_es_deinit(ctxt);
    ret = FALSE;
    goto done;
}

gboolean _agmp_es_create_vcaps(AgmpEsCtxt *ctxt)
{
    GstCaps *caps;
    AgmpVidCodecType codec = ctxt->v_path.cfgs.vcodec;
    gboolean ret;

    GST_TRACE("trace in");
    ret = TRUE;

    if (!ctxt->v_path.exist)
    {
        GST_DEBUG("vpath didn't exist. don't need to create vcaps");
        ret = TRUE;
        goto done;
    }

    caps = NULL;

    /* create from codec type */
    switch (codec)
    {
    case VCODEC_H264:
    {
        caps = gst_caps_new_simple("video/x-h264",
                                   "stream-format", G_TYPE_STRING, "byte-stream",
                                   //    "alignment", G_TYPE_STRING, "au", NULL);
                                   "alignment", G_TYPE_STRING, "nal", NULL);
        break;
    }
    case VCODEC_H265:
    {
        caps = gst_caps_new_empty_simple("video/x-h265");
    }
    case VCODEC_MPEG2:
    {
        caps = gst_caps_new_simple("video/mpeg", "mpegversion", G_TYPE_INT, 2, NULL);
        break;
    }
    case VCODEC_THEORA:
    {
        caps = gst_caps_new_empty_simple("video/x-theora");
        break;
    }
    case VCODEC_VC1:
    {
        caps = gst_caps_new_empty_simple("video/x-vc1");
        break;
    }
    case VCODEC_AV1:
    {
        caps = gst_caps_new_empty_simple("video/x-av1");
        break;
    }
    case VCODEC_VP8:
    {
        caps = gst_caps_new_empty_simple("video/x-vp8");
        break;
    }
    case VCODEC_VP9:
    {
        caps = gst_caps_new_empty_simple("video/x-vp9");
        break;
    }
    default:
    {
        GST_ERROR("meet unknown codec type");
        ret = FALSE;
        goto done;
    }
    }

    if (!caps)
    {
        GST_ERROR("create video caps error");
        ret = FALSE;
        goto done;
    }

    /* set secure caps feature if need */
    if (ctxt->common_cfgs.secure_mode)
    {
        gchar *sec_feature = NULL;
        switch (codec)
        {
        case VCODEC_H264:
        case VCODEC_H265:
        {
            sec_feature = GST_CAPS_FEATURE_MEMORY_SECMEM_MEMORY;
            break;
        }
        case VCODEC_AV1:
        case VCODEC_VP9:
        {
            sec_feature = GST_CAPS_FEATURE_MEMORY_DMABUF;
            break;
        }
        default:
        {
            GST_ERROR("only handle h264/h265/av1/vp9 enc for now");
            break;
        }
        }
        GST_DEBUG("add caps feature:%s", sec_feature);
        gst_caps_set_features_simple(caps, gst_caps_features_from_string(sec_feature));
    }

    /* update from format info */
    if (ctxt->v_path.cfgs.format_info.frame_width != 0 && ctxt->v_path.cfgs.format_info.frame_width != -1 &&
        ctxt->v_path.cfgs.format_info.frame_height != 0 && ctxt->v_path.cfgs.format_info.frame_height != -1)
    {
        gst_caps_set_simple(caps,
                            "width", G_TYPE_INT, ctxt->v_path.cfgs.format_info.frame_width,
                            "height", G_TYPE_INT, ctxt->v_path.cfgs.format_info.frame_height, NULL);
    }

    if (ctxt->v_path.cfgs.format_info.has_color_metadata)
        _agmp_es_update_vid_colormeta_into_caps(caps, &ctxt->v_path.cfgs.format_info.color_metadata);

    /* update new caps into ctxt */
    if (ctxt->v_path.caps)
        gst_caps_unref(ctxt->v_path.caps);
    ctxt->v_path.caps = caps;

    GST_DEBUG("create caps: %" GST_PTR_FORMAT, ctxt->v_path.caps);

done:
    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
}

gboolean _agmp_es_create_acaps(AgmpEsCtxt *ctxt)
{
    GstCaps *caps;
    AgmpAudCodecType codec = ctxt->a_path.cfgs.acodec;
    gboolean ret;

    GST_TRACE("trace in");

    ret = TRUE;

    if (!ctxt->a_path.exist)
    {
        GST_DEBUG("apath didn't exist. don't need to create acaps");
        ret = TRUE;
        goto done;
    }

    caps = NULL;

    /* create from codec type */
    switch (codec)
    {
    case ACODEC_AAC:
    {
        caps = gst_caps_new_simple("audio/mpeg", "mpegversion", G_TYPE_INT, 4, NULL);
        // TODO: need add rate & channel infos into caps
        break;
    }
    case ACODEC_AC3:
    case ACODEC_EAC3:
    {
        caps = gst_caps_new_empty_simple("audio/x-eac3");
        break;
    }
    case ACODEC_OPUS:
    {
        caps = gst_caps_new_simple("audio/x-opus", "channel-mapping-family", G_TYPE_INT, 0, NULL);
        // TODO: need construct codec data buf and push it into caps
        break;
    }
    case ACODEC_VORBIS:
    {
        caps = gst_caps_new_empty_simple("audio/x-vorbis");
        break;
    }
    case ACODEC_MP3:
    case ACODEC_FLAC:
    case ACODEC_PCM:
    {
        GST_ERROR("TODO: add audio codecs");
        break;
    }
    default:
    {
        GST_ERROR("meet unknown codec type");
        break;
    }
    }

    if (!caps)
    {
        GST_ERROR("create audio caps error");
        ret = FALSE;
        goto done;
    }

    /* update from format info */
    if (ACODEC_AAC == codec)
    {
        if (ctxt->a_path.cfgs.format_info.number_of_channels != -1 && ctxt->a_path.cfgs.format_info.number_of_channels != 0)
            gst_caps_set_simple(caps, "channels", G_TYPE_INT, ctxt->a_path.cfgs.format_info.number_of_channels, NULL);
        if (ctxt->a_path.cfgs.format_info.samples_per_second != -1 && ctxt->a_path.cfgs.format_info.samples_per_second != 0)
            gst_caps_set_simple(caps, "rate", G_TYPE_INT, ctxt->a_path.cfgs.format_info.samples_per_second, NULL);
    }
    else if (ACODEC_OPUS == codec)
    {
        uint16_t codec_priv_size = ctxt->a_path.cfgs.format_info.data.size;
        const void *codec_priv = ctxt->a_path.cfgs.format_info.data.data;
        if (codec_priv && codec_priv_size >= 19)
        {
            GstBuffer *tmp;
            GstCaps *new_caps;

            tmp = gst_buffer_new_wrapped(g_memdup(codec_priv, codec_priv_size), codec_priv_size);
            if (tmp)
            {
                new_caps = gst_codec_utils_opus_create_caps_from_header(tmp, NULL);
                if (new_caps)
                {
                    gst_caps_unref(caps);
                    caps = new_caps;
                }
                else
                    GST_ERROR("create caps form audio spec data error. use original caps");

                gst_buffer_unref(tmp);
            }
            else
            {
                GST_ERROR("create caps form audio spec data error. use original caps");
            }
        }
    }

    /* update new caps into ctxt */
    if (ctxt->a_path.caps)
        gst_caps_unref(ctxt->a_path.caps);
    ctxt->a_path.caps = caps;

    GST_DEBUG("create caps: %" GST_PTR_FORMAT, ctxt->a_path.caps);

    ret = TRUE;

done:
    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
}

gboolean _agmp_es_start_msg_thread(AgmpEsCtxt *ctxt)
{
    gboolean ret;

    GST_TRACE("trace in");

    ret = TRUE;

    AGMP_ASSERT_FAIL_RET(ctxt->main_loop_context && ctxt->main_loop, FALSE, "mainloop has not been created yet.");

    g_main_context_pop_thread_default(ctxt->main_loop_context);
    if (!(ctxt->msg_thread = g_thread_new("_agmp_es_msg_thread_func", (GThreadFunc)_agmp_es_msg_thread_func, (gpointer)ctxt)))
    {
        GST_ERROR("create thread error");
        g_main_context_push_thread_default(ctxt->main_loop_context);
        ret = FALSE;
        goto done;
    }

    while (!g_main_loop_is_running(ctxt->main_loop))
        g_usleep(1);

done:
    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
}

gboolean _agmp_es_start_data_ctl_thread(AgmpEsCtxt *ctxt)
{
    gboolean ret;

    GST_TRACE("trace in");

    ret = TRUE;

    if (!(ctxt->data_ctl_thread = g_thread_new("_agmp_es_data_ctl_thread_func", (GThreadFunc)_agmp_es_data_ctl_thread_func, (gpointer)ctxt)))
    {
        GST_ERROR("create thread error");
        ret = FALSE;
        goto done;
    }

done:
    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
}

gpointer _agmp_es_msg_thread_func(gpointer data)
{
    AgmpEsCtxt *ctxt;

    GST_TRACE("trace in");

    ctxt = (AgmpEsCtxt *)data;

    g_main_context_push_thread_default(ctxt->main_loop_context);

    _agmp_es_set_state(ctxt, AGMP_ES_STATE_INIT);

    g_main_loop_run(ctxt->main_loop);

    GST_TRACE("trace out ret ptr:%p", NULL);
    return NULL;
}

gpointer _agmp_es_data_ctl_thread_func(gpointer data)
{
    AgmpEsCtxt *ctxt;

    GST_TRACE("trace in");
    ctxt = (AgmpEsCtxt *)data;

    while (!ctxt->quit_data_ctl)
    {
        AgmpMsg msg;
        AgmpEsDataStatus v_status, a_status;

        memset(&msg, 0, sizeof(msg));

        if (GST_STATE(ctxt->pipeline) < GST_STATE_PAUSED)
        {
            GST_WARNING("pipeline is not in PAUSED or PLAYING state");
            g_usleep(AGMP_ES_CHECK_BUF_INTERVAL);
            continue;
        }

        if (!ctxt->paused_internal && GST_STATE(ctxt->pipeline) == GST_STATE_PAUSED)
        {
            GST_WARNING("pipeline is in PAUSED state based on upper-layer configuration");
            g_usleep(AGMP_ES_CHECK_BUF_INTERVAL);
            continue;
        }

        v_status = _agmp_es_data_status(ctxt, AGMP_VID);
        a_status = _agmp_es_data_status(ctxt, AGMP_AUD);

        if (ctxt->paused_internal)
        {
            if (AGMP_ES_DATA_BUFFERING_DONE == v_status && AGMP_ES_DATA_BUFFERING_DONE == a_status)
            {
                msg.type = AGMP_MSG_DATA_STAT_HIGH;
                _agmp_dispatch_msg_on_mainloop(ctxt, &msg);
                ctxt->paused_internal = FALSE;
                if (ctxt->play_state)
                {
                    _agmp_es_set_pipeline_state(ctxt, GST_STATE_PLAYING);
                    GST_INFO("data enough. Set Pipline to PLAYING internal");
                }
            }
            else
                GST_INFO("data not enough. Keep Pipline in PAUSED internal");
        }
        else
        {
            if (AGMP_ES_DATA_NEED_BUFFERING == v_status || AGMP_ES_DATA_NEED_BUFFERING == a_status)
            {
                GST_INFO("Set Pipline to PAUSE internal(v status:%d, a status:%d)", v_status, a_status);
                _agmp_es_set_pipeline_state(ctxt, GST_STATE_PAUSED);
                ctxt->paused_internal = TRUE;
            }
            else
                GST_INFO("data normal.");
        }
        GST_INFO("intenal pause:%d (v status:%d, a status:%d)", ctxt->paused_internal, v_status, a_status);

        if (AGMP_ES_DATA_BUFFERING_DONE != v_status || AGMP_ES_DATA_BUFFERING_DONE != a_status)
        {
            if (AGMP_ES_DATA_BUFFERING_DONE != v_status)
            {
                GST_INFO("acquire vid data triggered by data ctl");
                _agmp_es_appsrc_need_data((GstAppSrc *)ctxt->v_path.src, 0, ctxt);
            }
            if (AGMP_ES_DATA_BUFFERING_DONE != a_status)
            {
                GST_INFO("acquire aud data triggered by data ctl");
                _agmp_es_appsrc_need_data((GstAppSrc *)ctxt->a_path.src, 0, ctxt);
            }
            g_usleep(1);
        }
        else
            g_usleep(AGMP_ES_CHECK_BUF_INTERVAL);
    }

    GST_TRACE("trace out ret ptr:%p", NULL);
    return NULL;
}

gboolean _agmp_es_bus_cb(GstBus *bus, GstMessage *message, gpointer user_data)
{
    AgmpEsCtxt *ctxt;
    gboolean ret;

    GST_TRACE("trace in");
    GST_INFO("Got GST message %s from %s", GST_MESSAGE_TYPE_NAME(message), GST_MESSAGE_SRC_NAME(message));

    ctxt = (AgmpEsCtxt *)user_data;
    ret = TRUE;

    switch (GST_MESSAGE_TYPE(message))
    {
    case GST_MESSAGE_APPLICATION:
    {
        const GstStructure *structure = gst_message_get_structure(message);
        if (gst_structure_has_name(structure, "force-stop"))
        {
            if (ctxt->flow_force_stop)
                GST_INFO("already received force-stop msg. ignore this time");
            else
            {
                GST_INFO("receive force-stop msg");
                ctxt->flow_force_stop = TRUE;
                ret = _agmp_es_set_pipeline_state(ctxt, GST_STATE_READY);
            }
        }
        break;
    }
    case GST_MESSAGE_EOS:
    {
        if (GST_MESSAGE_SRC(message) == GST_OBJECT(ctxt->pipeline))
            ret = _agmp_es_set_state(ctxt, AGMP_ES_STATE_EOS);
        break;
    }
    case GST_MESSAGE_ERROR:
    {
        gboolean v_eos;
        gboolean a_eos;
        GError *err;
        gchar *debug;

        err = NULL;
        debug = NULL;
        gst_message_parse_error(message, &err, &debug);
        if (!err || !debug)
        {
            if (err)
                g_error_free(err);
            if (debug)
                g_free(debug);
            GST_ERROR("Parse error msg err");
            break;
        }

        v_eos = TRUE;
        a_eos = TRUE;
        if (ctxt->v_path.exist && !ctxt->v_path.src_data_eos)
            v_eos = FALSE;
        if (ctxt->a_path.exist && !ctxt->a_path.src_data_eos)
            a_eos = FALSE;

        if (err->domain == GST_STREAM_ERROR && (v_eos && a_eos))
        {
            GST_WARNING("Got stream error. But all streams are ended, so reporting EOS. Error code %d: %s (%s).", err->code, err->message, debug);
            ret = _agmp_es_set_state(ctxt, AGMP_ES_STATE_EOS);
        }
        else
        {
            GST_ERROR("Error %d: %s (%s)", err->code, err->message, debug);
            ret = _agmp_dispatch_error_msg(ctxt, AGMP_MSG_ERROR_DEC);
        }
        g_free(debug);
        g_error_free(err);
        break;
    }
    case GST_MESSAGE_STATE_CHANGED:
    {
        if (GST_MESSAGE_SRC(message) == GST_OBJECT(ctxt->pipeline))
        {
            GstState old_state, new_state, pending;
            gchar *dot_name;

            gst_message_parse_state_changed(message, &old_state, &new_state, &pending);
            GST_WARNING_OBJECT(GST_MESSAGE_SRC(message),
                               "Player_Status ===> State changed (old: %s, new: %s, pending: %s)",
                               gst_element_state_get_name(old_state),
                               gst_element_state_get_name(new_state),
                               gst_element_state_get_name(pending));

            dot_name = g_strdup_printf("agmp_es_%s-%s",
                                       gst_element_state_get_name(old_state),
                                       gst_element_state_get_name(new_state));
            GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(ctxt->pipeline), GST_DEBUG_GRAPH_SHOW_ALL, dot_name);
            g_free(dot_name);
        }
        break;
    }
    case GST_MESSAGE_ASYNC_DONE:
    {
        if (GST_MESSAGE_SRC(message) == GST_OBJECT(ctxt->pipeline))
        {
            GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(ctxt->pipeline), GST_DEBUG_GRAPH_SHOW_ALL, "agmp-es.async-done");
            GST_INFO("agmp-es state:%d, wait_preroll:%d", ctxt->state, ctxt->wait_preroll);
            if ((AGMP_ES_STATE_PREROLL_AFTER_SEEK == ctxt->state || AGMP_ES_STATE_PREROLL_INIT == ctxt->state) && ctxt->wait_preroll)
            {
                GST_DEBUG("pipeline has been asynchronously switched to the pause state");
                ret = _agmp_es_set_state(ctxt, AGMP_ES_STATE_PRESENT);
                ctxt->paused_internal = FALSE;
                ctxt->wait_preroll = FALSE;
            }
            else
                GST_ERROR("meet error here");
        }
        break;
    }
    case GST_MESSAGE_CLOCK_LOST:
    {
        GST_WARNING("GST_MESSAGE_CLOCK_LOST");
        if (!ctxt->paused_internal)
        {
            ret = _agmp_es_set_pipeline_state(ctxt, GST_STATE_PAUSED);
            ret &= _agmp_es_set_pipeline_state(ctxt, GST_STATE_PLAYING);
        }
        break;
    }
    case GST_MESSAGE_LATENCY:
    {
        gst_bin_recalculate_latency(GST_BIN(ctxt->pipeline));
        break;
    }
    case GST_MESSAGE_QOS:
    {
        const gchar *klass;
        klass = gst_element_class_get_metadata(GST_ELEMENT_GET_CLASS(GST_MESSAGE_SRC(message)), GST_ELEMENT_METADATA_KLASS);
        if (g_strrstr(klass, "Video"))
        {
            GstFormat format;
            guint64 dropped = 0;
            gst_message_parse_qos_stats(message, &format, NULL, &dropped);
            if (format == GST_FORMAT_BUFFERS)
            {
                ctxt->v_path.dropped_frame_num = dropped;
            }
        }
        break;
    }
    default:
    {
        GST_LOG("Got GST message %s from %s", GST_MESSAGE_TYPE_NAME(message), GST_MESSAGE_SRC_NAME(message));

        if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_INFO)
        {
            const GstStructure *info = NULL;
            gst_message_parse_info_details(message, &info);

            if (info && g_strcmp0(gst_structure_get_name(info), "segment-received") == 0)
            {
                GST_INFO("===> SEGMENT-DONE");
            }
        }
        break;
    }
    }

    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
}

gboolean _agmp_dispatch_data_msg(AgmpEsCtxt *ctxt, AgmpMsgType msg_type, AgmpEsType es_type, void *data_ptr)
{
    gboolean ret;
    AgmpMsg msg;

    GST_TRACE("trace in");

    if (G_UNLIKELY(msg_type < AGMP_MSG_DATA_NEED || msg_type > AGMP_MSG_DATA_STAT_HIGH))
    {
        GST_ERROR("error msg type:%d for data msg", msg_type);
        goto errors;
    }

    if (ctxt->common_cfgs.serial_data_mode && AGMP_MSG_DATA_NEED == msg_type)
    {
        gint *data_waiting = NULL;

        if (AGMP_VID == es_type)
            data_waiting = &ctxt->v_path.data_waiting;
        else if (AGMP_AUD == es_type)
            data_waiting = &ctxt->a_path.data_waiting;
        AGMP_ASSERT_FAIL_GOTO(data_waiting, errors, "meet error here. this ptr shouldn't be NULL.");

        if (*data_waiting)
        {
            GST_INFO("type:%d skip this time", es_type);
            ret = TRUE;
            goto done;
        }
        else
        {
            GST_INFO("type:%d schedule this time", es_type);
            g_atomic_int_set(data_waiting, 1);
        }
    }

    memset(&msg, 0, sizeof(msg));
    msg.type = msg_type;
    msg.body.data_state.type = es_type;
    msg.body.data_state.usr_data = data_ptr;
    ret = _agmp_dispatch_msg_on_mainloop(ctxt, &msg);

done:
    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
errors:
    ret = FALSE;
    goto done;
}

gboolean _agmp_dispatch_state_msg(AgmpEsCtxt *ctxt, AgmpMsgType msg_type)
{
    gboolean ret;

    GST_TRACE("trace in");

    if (G_UNLIKELY(msg_type < AGMP_MSG_STATE_INIT || msg_type > AGMP_MSG_STATE_DESTROY))
    {
        GST_ERROR("error msg type:%d for state msg", msg_type);
        goto errors;
    }

    ret = _agmp_dispatch_msg_uncheck(ctxt, msg_type);

done:
    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
errors:
    ret = FALSE;
    goto done;
}

gboolean _agmp_dispatch_status_msg(AgmpEsCtxt *ctxt, AgmpMsgType msg_type)
{
    gboolean ret;

    GST_TRACE("trace in");

    if (G_UNLIKELY(msg_type != AGMP_MSG_STATUS_UPDATE))
    {
        GST_ERROR("error msg type:%d for status msg", msg_type);
        goto errors;
    }

    ret = _agmp_dispatch_msg_uncheck(ctxt, msg_type);

done:
    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
errors:
    ret = FALSE;
    goto done;
}

gboolean _agmp_dispatch_error_msg(AgmpEsCtxt *ctxt, AgmpMsgType msg_type)
{
    gboolean ret;

    GST_TRACE("trace in");

    if (G_UNLIKELY(msg_type < AGMP_MSG_ERROR_DEC || msg_type > AGMP_MSG_ERROR_CAP_CHG))
    {
        GST_ERROR("error msg type:%d for error msg", msg_type);
        goto errors;
    }

    ret = _agmp_dispatch_msg_uncheck(ctxt, msg_type);

done:
    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
errors:
    ret = FALSE;
    goto done;
}

gboolean _agmp_dispatch_msg_uncheck(AgmpEsCtxt *ctxt, AgmpMsgType msg_type)
{
    gboolean ret;
    AgmpMsg msg;

    GST_TRACE("trace in");

    memset(&msg, 0, sizeof(msg));
    msg.type = msg_type;
    ret = _agmp_dispatch_msg_on_mainloop(ctxt, &msg);

    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
}

gboolean _agmp_dispatch_msg_on_mainloop(AgmpEsCtxt *ctxt, AgmpMsg *msg)
{
    GSource *src;
    AgmpMsg *msg_send;
    gboolean ret;
    gchar *extra_info;

    GST_TRACE("trace in");

    src = NULL;
    msg_send = NULL;
    ret = TRUE;
    extra_info = "void";

    AGMP_ASSERT_FAIL_GOTO(ctxt->main_loop, errors, "main loop didn't exist.");
    AGMP_ASSERT_FAIL_GOTO(ctxt->common_cfgs.msg_cb, errors, "upper-layer didn't set message cb. no need to attach gsource.");
    AGMP_ASSERT_FAIL_GOTO((msg_send = g_new0(AgmpMsg, 1)), errors, "new AgmpMsg failed.");
    AGMP_ASSERT_FAIL_GOTO((src = g_source_new(&agmp_es_gsource_funcs, sizeof(GSource))), errors, "create gsource failed.");

    g_source_set_ready_time(src, 0);
    g_source_set_priority(src, G_PRIORITY_DEFAULT);

    memcpy(msg_send, msg, sizeof(AgmpMsg));
    msg_send->agmp_handle = ctxt;
    msg_send->send_time = GST_TRACER_TS;
    msg_send->Scheduling_time = GST_CLOCK_TIME_NONE;
    msg_send->finish_time = GST_CLOCK_TIME_NONE;
    g_source_set_callback(src, (GSourceFunc)_agmp_es_gsource_cb, (gpointer)msg_send, (GDestroyNotify)g_free);

    if (_agmp_es_is_data_msg(msg_send->type))
    {
        if (AGMP_VID == msg_send->body.data_state.type)
            extra_info = "type vid";
        else if (AGMP_AUD == msg_send->body.data_state.type)
            extra_info = "type aud";
        else
            extra_info = "type error";
    }
    GST_TRACE("[msg send] msg:< %d - %s(%s)> send time: %" GST_TIME_FORMAT, msg_send->type, messages[msg_send->type].name, extra_info, GST_TIME_ARGS(msg_send->send_time));

    g_source_attach(src, ctxt->main_loop_context);
    g_source_unref(src);

    ret = TRUE;

done:
    GST_TRACE("trace out ret bool:%d", ret);
    return ret;

errors:
    if (msg_send)
        g_free(msg_send);
    if (src)
        g_source_unref(src);

    ret = FALSE;
    goto done;
}

gboolean _agmp_es_gsource_cb(gpointer msg)
{
    AgmpEsCtxt *ctxt;
    AgmpMsg *agmp_msg;
    GstClockTime msg_schedule_dur;
    GstClockTime msg_process_dur;
    gboolean ret;
    gchar *extra_info;

    GST_TRACE("trace in");

    msg_schedule_dur = msg_process_dur = GST_CLOCK_TIME_NONE;
    agmp_msg = (AgmpMsg *)msg;
    ctxt = (AgmpEsCtxt *)agmp_msg->agmp_handle;
    ret = TRUE;
    extra_info = "void";

    agmp_msg->Scheduling_time = GST_TRACER_TS;
    msg_schedule_dur = agmp_msg->Scheduling_time - agmp_msg->send_time;
    if (_agmp_es_is_data_msg(agmp_msg->type))
    {
        if (AGMP_VID == agmp_msg->body.data_state.type)
            extra_info = "type vid";
        else if (AGMP_AUD == agmp_msg->body.data_state.type)
            extra_info = "type aud";
        else
            extra_info = "type error";
    }
    GST_TRACE("[msg schedule] msg:< %d - %s(%s)> send time: %" GST_TIME_FORMAT " Scheduling time: %" GST_TIME_FORMAT " duration: %" GST_TIME_FORMAT,
              agmp_msg->type, messages[agmp_msg->type].name, extra_info,
              GST_TIME_ARGS(agmp_msg->send_time),
              GST_TIME_ARGS(agmp_msg->Scheduling_time),
              GST_TIME_ARGS(msg_schedule_dur));
    if (msg_schedule_dur > 500 * GST_MSECOND)
        GST_ERROR("[msg error] msg:< %d - %s(%s)> Scheduling time is too long", agmp_msg->type, messages[agmp_msg->type].name, extra_info);

    if (ctxt && ctxt->common_cfgs.msg_cb)
        (*ctxt->common_cfgs.msg_cb)(ctxt->common_cfgs.user_data, (void *)msg);

    agmp_msg->finish_time = GST_TRACER_TS;
    msg_process_dur = agmp_msg->finish_time - agmp_msg->send_time;
    GST_TRACE("[msg done] msg:< %d - %s(%s)> send time: %" GST_TIME_FORMAT " Scheduling time: %" GST_TIME_FORMAT " finish time: %" GST_TIME_FORMAT " duration: %" GST_TIME_FORMAT,
              agmp_msg->type, messages[agmp_msg->type].name, extra_info,
              GST_TIME_ARGS(agmp_msg->send_time),
              GST_TIME_ARGS(agmp_msg->Scheduling_time),
              GST_TIME_ARGS(agmp_msg->finish_time),
              GST_TIME_ARGS(msg_process_dur));
    if (msg_process_dur > 500 * GST_MSECOND)
        GST_ERROR("[msg error] msg:< %d - %s(%s)> Processing time is too long", agmp_msg->type, messages[agmp_msg->type].name, extra_info);

    if (_agmp_es_is_state_msg(agmp_msg->type))
    {
        switch (agmp_msg->type)
        {
        case AGMP_MSG_STATE_DESTROY:
        {
            GST_INFO("quit main loop when meet msg:AGMP_MSG_STATE_DESTROY");
            g_main_loop_quit(ctxt->main_loop);
            break;
        }
        default:
            break;
        }
    }

    ret = G_SOURCE_REMOVE;

    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
}

static gboolean _agmp_es_set_state(AgmpEsCtxt *ctxt, AgmpEsStateType state)
{
    gboolean ret;
    AgmpMsgType msg_type;

    GST_TRACE("trace in | set state to %d", state);

    ret = FALSE;

    if (state == ctxt->state)
    {
        GST_DEBUG("state not change.");
        ret = TRUE;
        goto done;
    }

    ctxt->state = state;

    if (AGMP_ES_STATE_INIT == state)
        msg_type = AGMP_MSG_STATE_INIT;
    else if (AGMP_ES_STATE_PREROLL_INIT == state || AGMP_ES_STATE_PREROLL_AFTER_SEEK == state)
        msg_type = AGMP_MSG_STATE_PREROLL;
    else if (AGMP_ES_STATE_PRESENT == state)
        msg_type = AGMP_MSG_STATE_PRESENT;
    else if (AGMP_ES_STATE_EOS == state)
        msg_type = AGMP_MSG_STATE_EOS;
    else if (AGMP_ES_STATE_DESTROY == state)
        msg_type = AGMP_MSG_STATE_DESTROY;
    else
    {
    }

    ret = _agmp_dispatch_state_msg(ctxt, msg_type);

done:
    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
}

gboolean _agmp_es_set_pipeline_state(AgmpEsCtxt *ctxt, GstState state)
{
    GstStateChangeReturn result;
    gboolean ret;

    GST_TRACE("trace in");

    result = GST_STATE_CHANGE_SUCCESS;
    ret = TRUE;

    GST_DEBUG("setting pipeline to state:%s", gst_element_state_get_name(state));

    if (ctxt->flow_force_stop && state > GST_STATE_READY)
    {
        GST_INFO("Ignore state change due to forced-stop msg");
        ret = TRUE;
        goto done;
    }

    result = gst_element_set_state(ctxt->pipeline, state);
    if ((GST_STATE(ctxt->pipeline) < GST_STATE_PAUSED) && (state == GST_STATE_PAUSED))
    {
        if (result == GST_STATE_CHANGE_ASYNC)
            ctxt->wait_preroll = TRUE;
        else
            ctxt->wait_preroll = FALSE;
    }

    GST_DEBUG("setted pipeline to state:%s, need_preroll_:%d, return:%s",
              gst_element_state_get_name(state),
              ctxt->wait_preroll,
              gst_element_state_change_return_get_name(result));

    ret = (result != GST_STATE_CHANGE_FAILURE);

done:
    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
}

gboolean _agmp_es_write_v(AgmpEsCtxt *ctxt, AgmpDataInfo *data_info)
{
    GstBuffer *buf;
    gboolean ret;

    GST_TRACE("trace in");

    AGMP_ASSERT_FAIL_GOTO(ctxt->v_path.exist, errors, "vpath not exist");

    ctxt->v_path.total_frame_num++;

    /* update flags for serial data mode */
    g_atomic_int_set(&ctxt->v_path.data_waiting, 0);

    /* construct buf */
    AGMP_ASSERT_FAIL_GOTO((buf = _agmp_es_create_buf(ctxt, data_info)), errors, "create gst vid buf meet error.");

    /* decrypt buf */
    AGMP_ASSERT_FAIL_GOTO((_agmp_es_decrypt(ctxt, &buf)), errors, "decrypt gst vid buf meet error.");

    /* free input sample */
    ret = _agmp_dispatch_data_msg(ctxt, AGMP_MSG_DATA_RELEASE, AGMP_VID, data_info->usr_data);
    AGMP_ASSERT_FAIL_GOTO(ret, errors, "release vid meet error");

    /* push buf into pipeline*/
    GST_INFO("push vid buf into appsrc with TS %" GST_TIME_FORMAT " cur max TS %" GST_TIME_FORMAT,
             GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(buf)), GST_TIME_ARGS(ctxt->v_path.max_ts));

    gst_app_src_push_buffer(GST_APP_SRC(ctxt->v_path.src), buf);

    if (G_UNLIKELY(GST_CLOCK_TIME_NONE == ctxt->v_path.max_ts))
        ctxt->v_path.max_ts = GST_BUFFER_TIMESTAMP(buf);
    else
        ctxt->v_path.max_ts = ctxt->v_path.max_ts > GST_BUFFER_TIMESTAMP(buf) ? ctxt->v_path.max_ts : GST_BUFFER_TIMESTAMP(buf);

    ret = TRUE;

done:
    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
errors:
    ret = FALSE;
    goto done;
}

gboolean _agmp_es_write_a(AgmpEsCtxt *ctxt, AgmpDataInfo *data_info)
{
    GstElement *src;
    GstBuffer *buf;
    gboolean ret;

    GST_TRACE("trace in");

    AGMP_ASSERT_FAIL_GOTO(ctxt->a_path.exist, errors, "apath not exist");

    /* update flags for serial data mode */
    g_atomic_int_set(&ctxt->a_path.data_waiting, 0);

    /* construct buf */
    src = ctxt->a_path.src;
    buf = gst_buffer_new_allocate(NULL, data_info->size, NULL);
    gst_buffer_fill(buf, 0, data_info->data, data_info->size);
    GST_BUFFER_TIMESTAMP(buf) = data_info->timestamp;

    /* free input sample */
    ret = _agmp_dispatch_data_msg(ctxt, AGMP_MSG_DATA_RELEASE, AGMP_AUD, data_info->usr_data);
    AGMP_ASSERT_FAIL_GOTO(ret, errors, "release aud meet error");

    /* push buf into pipeline */
    GST_INFO("push aud buf into appsrc with TS %" GST_TIME_FORMAT " cur max TS %" GST_TIME_FORMAT,
             GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(buf)), GST_TIME_ARGS(ctxt->a_path.max_ts));

    gst_app_src_push_buffer(GST_APP_SRC(src), buf);

    if (G_UNLIKELY(GST_CLOCK_TIME_NONE == ctxt->a_path.max_ts))
        ctxt->a_path.max_ts = GST_BUFFER_TIMESTAMP(buf);
    else
        ctxt->a_path.max_ts = ctxt->a_path.max_ts > GST_BUFFER_TIMESTAMP(buf) ? ctxt->a_path.max_ts : GST_BUFFER_TIMESTAMP(buf);

    ret = TRUE;

done:
    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
errors:
    ret = FALSE;
    goto done;
}

void _agmp_es_free_data_info(AgmpEsCtxt *ctxt, AgmpDataInfo *data_info)
{
    GST_TRACE("trace in");

    if (AGMP_VID == data_info->type && data_info->drm_info.exist && data_info->drm_info.subsample_mapping)
        free(data_info->drm_info.subsample_mapping);

    GST_TRACE("trace out ret void");
}

GstBuffer *_agmp_es_create_buf(AgmpEsCtxt *ctxt, AgmpDataInfo *data_info)
{
    GstBuffer *buf;
    GstBuffer *key;
    GstBuffer *iv;
    GstBuffer *subsamples;
    GstBuffer *sec_buf;

    GST_TRACE("trace in");

    buf = NULL;
    key = iv = subsamples = sec_buf = NULL;

    AGMP_ASSERT_FAIL_GOTO((buf = gst_buffer_new_allocate(NULL, data_info->size, NULL)), errors, "create buf failed.");
    gst_buffer_fill(buf, 0, data_info->data, data_info->size);
    GST_BUFFER_TIMESTAMP(buf) = data_info->timestamp;

    if (!ctxt->common_cfgs.secure_mode)
    {
        GST_INFO("non-secure mode with clear sample(type:%d)", data_info->type);
        goto done;
    }
    if (!data_info->drm_info.exist)
    {
        GST_INFO("secure mode with clear sample(type:%d)", data_info->type);
        if (AGMP_VID == data_info->type)
        {
            AGMP_ASSERT_FAIL_GOTO((sec_buf = gst_buffer_new_allocate(ctxt->v_path.sec_allocator, gst_buffer_get_size(buf), NULL)), errors, "create sec buf meet error.");
            gst_buffer_copy_into(sec_buf, buf, (GstBufferCopyFlags)(GST_BUFFER_COPY_FLAGS | GST_BUFFER_COPY_TIMESTAMPS), 0, -1);
            gst_buffer_copy_to_secmem(sec_buf, buf);
            gst_buffer_unref(buf);
            buf = sec_buf;
        }
        goto done;
    }
    else
    {
        /* add drm info in buf */
        GstStructure *drm_info;

        AGMP_ASSERT_FAIL_GOTO(ctxt->v_path.sec_allocator, errors, "secure allocator is empty.");
        AGMP_ASSERT_FAIL_GOTO((key = _agmp_es_create_key(ctxt, &(data_info->drm_info))), errors, "create key meet error.");
        AGMP_ASSERT_FAIL_GOTO((iv = _agmp_es_create_iv(ctxt, &(data_info->drm_info))), errors, "create iv meet error.");
        AGMP_ASSERT_FAIL_GOTO((subsamples = _agmp_es_create_subsamples(ctxt, &(data_info->drm_info))), errors, "create subsamples meet error.");
        if (AGMP_VID == data_info->type)
        {
            AGMP_ASSERT_FAIL_GOTO((sec_buf = gst_buffer_new_allocate(ctxt->v_path.sec_allocator, gst_buffer_get_size(buf), NULL)), errors, "create sec buf meet error.");
            gst_buffer_copy_into(sec_buf, buf, (GstBufferCopyFlags)(GST_BUFFER_COPY_FLAGS | GST_BUFFER_COPY_TIMESTAMPS), 0, -1);
        }
        else
            sec_buf = NULL;
        drm_info = gst_structure_new("drm_info",
                                     "es_type", G_TYPE_INT, data_info->type,
                                     "scheme", G_TYPE_INT, data_info->drm_info.enc_scheme,
                                     "pattern_crypt_blocksize", G_TYPE_INT, data_info->drm_info.enc_pattern.crypt_byte_block,
                                     "pattern_skip_blocksize", G_TYPE_INT, data_info->drm_info.enc_pattern.skip_byte_block,
                                     "subsample_cnt", G_TYPE_INT, data_info->drm_info.subsample_count,
                                     "sec_buf", GST_TYPE_BUFFER, sec_buf,
                                     "key", GST_TYPE_BUFFER, key,
                                     "iv", GST_TYPE_BUFFER, iv,
                                     "subsamples", GST_TYPE_BUFFER, subsamples,
                                     NULL);

        gst_buffer_add_protection_meta(buf, drm_info);

        gst_buffer_unref(key);
        gst_buffer_unref(iv);
        gst_buffer_unref(subsamples);
        if (sec_buf)
            gst_buffer_unref(sec_buf);
    }

done:
    GST_TRACE("trace out ret ptr:%p", buf);
    return buf;
errors:
    if (key)
        gst_buffer_unref(key);
    if (iv)
        gst_buffer_unref(iv);
    if (subsamples)
        gst_buffer_unref(subsamples);
    if (buf)
        gst_buffer_unref(buf);
    buf = NULL;
    goto done;
}

GstBuffer *_agmp_es_create_key(AgmpEsCtxt *ctxt, AgmpDrmDataInfo *drm_info)
{
    GstBuffer *key;
    GST_TRACE("trace in");

    key = gst_buffer_new_allocate(NULL, drm_info->id_size, NULL);
    gst_buffer_fill(key, 0, drm_info->id, drm_info->id_size);

    GST_TRACE("trace out ret ptr:%p", key);
    return key;
}

GstBuffer *_agmp_es_create_iv(AgmpEsCtxt *ctxt, AgmpDrmDataInfo *drm_info)
{
    GstBuffer *iv;
    gint iv_size;

    GST_TRACE("trace in");

    iv_size = drm_info->iv_size;
#define kMaxIvSize 16
    const int8_t kEmptyArray[kMaxIvSize / 2] = {0};
    if (iv_size == kMaxIvSize && memcmp(drm_info->iv + kMaxIvSize / 2, kEmptyArray, kMaxIvSize / 2) == 0)
        iv_size /= 2;
    iv = gst_buffer_new_allocate(NULL, iv_size, NULL);
    gst_buffer_fill(iv, 0, drm_info->iv, iv_size);

    GST_TRACE("trace out ret ptr:%p", iv);
    return iv;
}

GstBuffer *_agmp_es_create_subsamples(AgmpEsCtxt *ctxt, AgmpDrmDataInfo *drm_info)
{
    GstBuffer *subsamples;
    gint subsamples_count;
    gint subsamples_raw_size;
    guint8 *subsamples_raw;
    GstByteWriter writer;

    GST_TRACE("trace in");

    subsamples = NULL;

    subsamples_count = drm_info->subsample_count;
    subsamples_raw_size = subsamples_count * (sizeof(guint16) + sizeof(guint32));
    subsamples_raw = (guint8 *)g_malloc(subsamples_raw_size);
    gst_byte_writer_init_with_data(&writer, subsamples_raw, subsamples_raw_size, FALSE);
    for (gint i = 0; i < subsamples_count; ++i)
    {
        if (!gst_byte_writer_put_uint16_be(&writer, drm_info->subsample_mapping[i].clear_byte_count))
        {
            GST_ERROR("Failed writing clear subsample info at %d", i);
            goto done;
        }
        if (!gst_byte_writer_put_uint32_be(&writer, drm_info->subsample_mapping[i].encrypted_byte_count))
        {
            GST_ERROR("Failed writing encrypted subsample info at %d", i);
            goto done;
        }
    }
    subsamples = gst_buffer_new_wrapped(subsamples_raw, subsamples_raw_size);

done:
    GST_TRACE("trace out ret ptr:%p", subsamples);
    return subsamples;
}

gboolean _agmp_es_decrypt(AgmpEsCtxt *ctxt, GstBuffer **buf)
{
    gboolean ret;
    GstProtectionMeta *protectionMeta;

    GST_TRACE("trace in");

    protectionMeta = NULL;
    ret = TRUE;

    AGMP_ASSERT_FAIL_GOTO((buf && *buf), errors, "input buf is invalid");

    protectionMeta = (GstProtectionMeta *)(gst_buffer_get_protection_meta(*buf));
    if (!protectionMeta)
    {
        GST_DEBUG("meet non-secure gst buf. No need to decrypt");
        ret = TRUE;
        goto done;
    }

    if (ctxt->common_cfgs.decrypt)
        ret = _agmp_es_decrypt_external(ctxt, buf);
    else
    {
        // TODO:need to needs to be supplement process of decryption by pipeline
        ret = TRUE;
    }

done:
    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
errors:
    ret = FALSE;
    goto done;
}

gboolean _agmp_es_decrypt_external(AgmpEsCtxt *ctxt, GstBuffer **buf)
{
    gboolean ret;
    GstProtectionMeta *protectionMeta;
    AgmpEsType type;
    secmem_handle_t handle;
    GstBuffer *sec_buf;
    GstBuffer *key;
    GstBuffer *iv;
    GstBuffer *subsamples;
    AgmpDrmEncScheme enc_scheme;
    AgmpDrmEncPattern enc_pattern;
    gint subsample_cnt;
    GstMapInfo dataMap;
    GstMapInfo keyMap;
    GstMapInfo ivMap;
    GstMapInfo subsamplesMap;
    GstByteReader *reader;

    GST_TRACE("trace in");

    ret = TRUE;
    protectionMeta = NULL;
    type = AGMP_NONE;
    handle = 0;
    sec_buf = NULL;
    key = NULL;
    iv = NULL;
    subsamples = NULL;
    enc_scheme = AGMP_ENC_SCHEME_NONE;
    enc_pattern.crypt_byte_block = 0;
    enc_pattern.skip_byte_block = 0;
    subsample_cnt = 0;
    memset(&dataMap, 0, sizeof(dataMap));
    memset(&keyMap, 0, sizeof(keyMap));
    memset(&ivMap, 0, sizeof(ivMap));
    memset(&subsamplesMap, 0, sizeof(subsamplesMap));
    reader = NULL;

    AGMP_ASSERT_FAIL_GOTO((buf && *buf), errors, "input buf is invalid");
    AGMP_ASSERT_FAIL_GOTO(ctxt->common_cfgs.decrypt, errors, "No external decryption function is configured");

    protectionMeta = (GstProtectionMeta *)(gst_buffer_get_protection_meta(*buf));
    if (!protectionMeta)
    {
        GST_DEBUG("meet non-secure gst buf. No need to decrypt");
        ret = TRUE;
        goto done;
    }
    AGMP_ASSERT_FAIL_GOTO(protectionMeta->info, errors, "Missing struct in protection info");

    /* Extract encrypted information */
    gst_structure_get_int(protectionMeta->info, "es_type", (gint *)(&type));
    gst_structure_get_int(protectionMeta->info, "scheme", (gint *)(&enc_scheme));
    gst_structure_get_int(protectionMeta->info, "pattern_crypt_blocksize", (gint *)(&(enc_pattern.crypt_byte_block)));
    gst_structure_get_int(protectionMeta->info, "pattern_skip_blocksize", (gint *)(&(enc_pattern.skip_byte_block)));
    gst_structure_get_int(protectionMeta->info, "subsample_cnt", &subsample_cnt);
    sec_buf = _agmp_es_extract_buf_from_struct(protectionMeta->info, "sec_buf");
    key = _agmp_es_extract_buf_from_struct(protectionMeta->info, "key");
    iv = _agmp_es_extract_buf_from_struct(protectionMeta->info, "iv");
    subsamples = _agmp_es_extract_buf_from_struct(protectionMeta->info, "subsamples");
    AGMP_ASSERT_FAIL_GOTO((key && iv && subsamples), errors, "Missing necessary data for decryption");

    if (sec_buf)
    {
        GstMemory *sec_mem;
        AGMP_ASSERT_FAIL_GOTO((sec_mem = gst_buffer_peek_memory(sec_buf, 0)), errors, "peek sec mem for sec buf meet error.");
        AGMP_ASSERT_FAIL_GOTO((handle = gst_secmem_memory_get_handle(sec_mem)), errors, "get sec handle for sec mem meet error.");
    }
    else
        handle = 0;
    AGMP_ASSERT_FAIL_GOTO(gst_buffer_map(*buf, &dataMap, (GstMapFlags)GST_MAP_READWRITE), errors, "map gst data buf meet error");
    AGMP_ASSERT_FAIL_GOTO(gst_buffer_map(key, &keyMap, (GstMapFlags)GST_MAP_READWRITE), errors, "map gst key buf meet error");
    AGMP_ASSERT_FAIL_GOTO(gst_buffer_map(iv, &ivMap, (GstMapFlags)GST_MAP_READWRITE), errors, "map gst iv buf meet error");
    AGMP_ASSERT_FAIL_GOTO(gst_buffer_map(subsamples, &subsamplesMap, (GstMapFlags)GST_MAP_READWRITE), errors, "map gst subsamples buf meet error");

    /* do decryption */
    reader = gst_byte_reader_new(subsamplesMap.data, subsamplesMap.size);
    uint16_t inClear = 0;
    uint32_t inEncrypted = 0;
    uint32_t totalEncrypted = 0;
    for (guint position = 0; position < subsample_cnt; position++)
    {
        gst_byte_reader_get_uint16_be(reader, &inClear);
        gst_byte_reader_get_uint32_be(reader, &inEncrypted);
        GST_DEBUG("inclear = [%d],inEncrypted=[%d]", inClear, inEncrypted);
        totalEncrypted += inEncrypted;
    }
    GST_DEBUG("totalEncrypted = [%d]", totalEncrypted);

    ret = (*ctxt->common_cfgs.decrypt)(ctxt->common_cfgs.user_data, type,
                                       (uint32_t)enc_scheme,
                                       (uint32_t)enc_pattern.crypt_byte_block, (uint32_t)enc_pattern.skip_byte_block,
                                       (uint8_t *)dataMap.data, (uint32_t)dataMap.size,
                                       (uint8_t *)keyMap.data, (uint32_t)keyMap.size,
                                       (uint8_t *)ivMap.data, (uint32_t)ivMap.size,
                                       (uint8_t *)subsamplesMap.data, (uint32_t)subsamplesMap.size, (uint32_t)subsample_cnt,
                                       (uint32_t)handle);
    AGMP_ASSERT_FAIL_GOTO(ret, errors, "Decryption failed");

    GST_DEBUG("Decryption successful");

    if (AGMP_VID == type)
    {
        GstMemory *mem = gst_buffer_peek_memory(sec_buf, 0);
        if (VCODEC_VP9 == ctxt->v_path.cfgs.vcodec)
        {
            GST_DEBUG("add header for vp9");
            gst_secmem_parse_vp9(mem);
        }
        else if (VCODEC_AV1 == ctxt->v_path.cfgs.vcodec)
        {
            GST_DEBUG("add header for av1");
            gst_secmem_parse_av1(mem);
        }
        /* ref sec_buf before unref buf */
        gst_buffer_ref(sec_buf);
        gst_buffer_unmap(*buf, &dataMap);
        gst_buffer_unref(*buf);
        *buf = sec_buf;
    }

    ret = TRUE;

done:
{
    if (reader)
    {
        gst_byte_reader_set_pos(reader, 0);
        gst_byte_reader_free(reader);
    }
    if (key && keyMap.data)
        gst_buffer_unmap(key, &keyMap);
    if (iv && ivMap.data)
        gst_buffer_unmap(iv, &ivMap);
    if (subsamples && subsamplesMap.data)
        gst_buffer_unmap(subsamples, &subsamplesMap);

    /* All bufs associated in the meta will be unrefed at this time */
    gst_buffer_remove_meta(*buf, (GstMeta *)protectionMeta);
    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
}
errors:
{
    if (*buf && dataMap.data)
        gst_buffer_unmap(*buf, &dataMap);
    ret = FALSE;
    goto done;
}
}

GstBuffer *_agmp_es_extract_buf_from_struct(GstStructure *s, const gchar *name)
{
    GstBuffer *buf;
    const GValue *value;

    GST_TRACE("trace in");
    GST_INFO("extract_buf:%s", name);

    buf = NULL;

    AGMP_ASSERT_FAIL_GOTO((s && name), done, "input params error");

    value = gst_structure_get_value(s, name);
    if (!value)
        goto done;
    buf = gst_value_get_buffer(value);

done:
    GST_TRACE("trace out ret ptr:%p", buf);
    return buf;
}

void _agmp_es_appsrc_need_data(GstAppSrc *src, guint length, gpointer user_data)
{
    AgmpEsCtxt *ctxt;
    AgmpEsType type;

    GST_TRACE("trace in");

    ctxt = (AgmpEsCtxt *)user_data;

    type = _agmp_es_appsrc_media_type(ctxt, src);
    if (AGMP_VID == type)
        ctxt->v_path.src_data_enough = FALSE;
    else if (AGMP_AUD == type)
        ctxt->a_path.src_data_enough = FALSE;

#if 0
    if (AGMP_VID == type && ctxt->v_path.cfgs.secure_mode && ctxt->v_path.sec_ctxt)
    {
        if (!agmp_es_sec_res_available(ctxt->v_path.sec_ctxt))
        {
            GST_LOG_OBJECT(src, "start polling secmem availability");
            _agmp_es_add_monitor(ctxt, AGMP_ES_MONITOR_SECMEM);
            return;
        }
    }
#endif

#if 0
    if (AGMP_ES_STATE_PREROLL_AFTER_SEEK == ctxt->state && _agmp_es_data_has_enough(ctxt))
    {
        /*
            表示在seek之后有一路已经把appsrc queue占满了，另外一路还未占满，这时就不需要再去读数据了
            我理解如果两个appsrc的queue size配置合适的话，不会遇到这种情况
        */
        GST_LOG_OBJECT(src, "Seeking. Waiting for other appsrcs.");
        return;
    }
#endif

    _agmp_dispatch_data_msg(ctxt, AGMP_MSG_DATA_NEED, type, NULL);

    GST_TRACE("trace out ret void");
}

void _agmp_es_appsrc_enough_data(GstAppSrc *src, gpointer user_data)
{
    AgmpEsCtxt *ctxt;
    AgmpEsType type;

    GST_TRACE("trace in");

    ctxt = (AgmpEsCtxt *)user_data;
    type = _agmp_es_appsrc_media_type(ctxt, src);

    if (AGMP_VID == type)
        ctxt->v_path.src_data_enough = TRUE;
    else if (AGMP_AUD == type)
        ctxt->v_path.src_data_enough = TRUE;

    _agmp_dispatch_data_msg(ctxt, AGMP_MSG_DATA_ENOUGH, type, NULL);

    GST_TRACE("trace out ret void");
}

gboolean _agmp_es_appsrc_seek_data(GstAppSrc *src, guint64 offset, gpointer user_data)
{
    AgmpEsCtxt *ctxt;
    gboolean ret;

    GST_TRACE("trace in");

    ctxt = (AgmpEsCtxt *)user_data;
    ret = TRUE;

    if (AGMP_ES_STATE_PREROLL_AFTER_SEEK != ctxt->state)
    {
        GST_DEBUG_OBJECT(src, "Not seeking");
        ret = TRUE;
        goto done;
    }

    // TODO:why enough?
    _agmp_es_appsrc_enough_data(src, user_data);

    ret = TRUE;

done:
    GST_TRACE("trace out ret bool:%d", ret);
    return ret;
}

inline AgmpEsType _agmp_es_appsrc_media_type(AgmpEsCtxt *ctxt, GstAppSrc *src)
{
    if (NULL == src)
    {
        GST_ERROR("input src is NULL");
        return AGMP_NONE;
    }

    if ((gpointer)ctxt->v_path.src == (gpointer)src)
        return AGMP_VID;
    if ((gpointer)ctxt->a_path.src == (gpointer)src)
        return AGMP_AUD;
    return AGMP_NONE;
}

#if 0
void _agmp_es_underflow_cb(GstElement *object, guint arg0, gpointer arg1, gpointer user_data)
{
    AgmpEsCtxt *ctxt;
    AgmpEsType es_type;
    gboolean src_data_eos;
    AgmpMsg msg;

    es_type = AGMP_NONE;
    ctxt = (AgmpEsCtxt *)user_data;
    memset(&msg, 0, sizeof(msg));

    if (object == ctxt->v_path.sink)
    {
        es_type = AGMP_VID;
        src_data_eos = ctxt->v_path.src_data_eos;
        msg.type = AGMP_MSG_DATA_VID_UNDERFLOW;
    }
    if (object == ctxt->a_path.sink)
    {
        es_type = AGMP_AUD;
        src_data_eos = ctxt->a_path.src_data_eos;
        msg.type = AGMP_MSG_DATA_AUD_UNDERFLOW;
    }
    if (AGMP_NONE == es_type)
    {
        GST_ERROR("find wrong es type");
        return;
    }

    if (GST_STATE(ctxt->pipeline) != GST_STATE_PLAYING)
    {
        GST_WARNING("Player_Status under flow callback happened when pipeline not in PLAYING state");
    }
    else
    {
        /*
            Meet video not eos and query video underflow
            Need pause pipeline due to video underflow
        */
        if (!src_data_eos)
        {
            _agmp_es_set_pipeline_state(ctxt, GST_STATE_PAUSED);
            ctxt->paused_internal = TRUE;
        }

        _agmp_dispatch_msg_on_mainloop(ctxt, &msg);
    }
}
#endif

gboolean _agmp_es_add_monitor(AgmpEsCtxt *ctxt, AgmpEsMonitorType monitor_type)
{
    gint64 timeout_val;
    GSource **src;
    GSource *new_src;
    AgmpMonitorData *monitor_data;

    GST_TRACE("trace in");

    timeout_val = -1;
    src = NULL;
    new_src = NULL;
    monitor_data = NULL;

    switch (monitor_type)
    {
    case AGMP_ES_MONITOR_STATUS:
    {
        if (ctxt->common_cfgs.status_update_interval)
        {
            _agmp_es_remove_monitor(ctxt, AGMP_ES_MONITOR_STATUS);
            timeout_val = ctxt->common_cfgs.status_update_interval;
            src = &ctxt->player_status_monitor;
            GST_DEBUG("try to create player status monitor with interval :%lld", timeout_val);
        }
        break;
    }
#if 0
    case AGMP_ES_MONITOR_SECMEM:
    {
        if (sec_ctxt->secmem_status_monitor)
            _agmp_es_remove_monitor(ctxt, AGMP_ES_MONITOR_SECMEM);

        timeout_val = 100;
        if (getenv("SECMEM_POLLING_INTERVAL"))
        {
            timeout_val = atoi(getenv("SECMEM_POLLING_INTERVAL"));
        }

        GST_DEBUG("try to create secure memory status monitor with interval :%lld", timeout_val);
        break;
    }
#endif
    default:
    {
        GST_ERROR("invalid monitor type");
        goto errors;
    }
    }

    AGMP_ASSERT_FAIL_GOTO(src, errors, "meet error here.");

    GST_DEBUG("new monitor type:%d with timeout %lld(ms)", monitor_type, timeout_val);

    AGMP_ASSERT_FAIL_GOTO((monitor_data = g_new0(AgmpMonitorData, 1)), errors, "new AgmpMonitorData failed.");
    monitor_data->monitor_type = monitor_type;
    monitor_data->ctxt = ctxt;

    AGMP_ASSERT_FAIL_GOTO((new_src = g_timeout_source_new(timeout_val)), errors, "create monitor failed.");
    g_source_set_priority(new_src, G_PRIORITY_DEFAULT);
    g_source_set_callback(new_src, (GSourceFunc)_agmp_es_monitor_cb, monitor_data, (GDestroyNotify)g_free);
    g_source_attach(new_src, ctxt->main_loop_context);

    *src = new_src;

    return TRUE;

errors:
    if (monitor_data)
        g_free(monitor_data);
    return FALSE;
}

void _agmp_es_remove_monitor(AgmpEsCtxt *ctxt, AgmpEsMonitorType monitor_type)
{
    GSource **src;

    GST_TRACE("trace in");

    src = NULL;

    switch (monitor_type)
    {
    case AGMP_ES_MONITOR_STATUS:
    {
        if (ctxt->common_cfgs.status_update_interval && ctxt->player_status_monitor)
            src = &ctxt->player_status_monitor;
        break;
    }
#if 0
    case AGMP_ES_MONITOR_SECMEM:
    {
        if (sec_ctxt->secmem_status_monitor)
            src = &ctxt->secmem_status_monitor;
        break;
    }
#endif
    default:
    {
        GST_ERROR("invalid monitor type");
        break;
    }
    }

    if (!src)
    {
        GST_DEBUG("This type of monitor has not been created yet");
        return;
    }

    if (*src)
    {
        g_source_destroy(*src);
        g_source_unref(*src);
        *src = NULL;
    }
}

gboolean _agmp_es_monitor_cb(AgmpMonitorData *monitor_data)
{
    AgmpEsMonitorType monitor_type;
    AgmpEsCtxt *ctxt;

    GST_TRACE("trace in");

    monitor_type = monitor_data->monitor_type;
    ctxt = monitor_data->ctxt;

    switch (monitor_type)
    {
    case AGMP_ES_MONITOR_STATUS:
    {
        _agmp_dispatch_status_msg(ctxt, AGMP_MSG_STATUS_UPDATE);
        return G_SOURCE_CONTINUE;
    }
#if 0
    case AGMP_ES_MONITOR_SECMEM:
    {
        return agmp_es_sec_check_mem_available(ctxt->sec_ctxt);
    }
#endif
    default:
    {
        GST_ERROR("invalid monitor type");
        return G_SOURCE_REMOVE;
    }
    }
}

AgmpEsDataStatus _agmp_es_data_status(AgmpEsCtxt *ctxt, AgmpEsType type)
{
    gboolean *is_exist;
    gboolean *is_eos;
    gboolean *is_enough;
    GstClockTime *max_ts;
    GstClockTime *min;
    GstClockTime *max;
    GstClockTime position;
    AgmpEsDataStatus status;

    GST_TRACE("trace in");

    is_exist = is_eos = is_enough = NULL;
    max_ts = min = max = NULL;

    switch (type)
    {
    case AGMP_VID:
    {
        is_exist = &ctxt->v_path.exist;
        is_eos = &ctxt->v_path.src_data_eos;
        is_enough = &ctxt->v_path.src_data_enough;
        max_ts = &ctxt->v_path.max_ts;
        min = &ctxt->data_ctl.min_v;
        max = &ctxt->data_ctl.max_v;
        break;
    };
    case AGMP_AUD:
    {
        is_exist = &ctxt->a_path.exist;
        is_eos = &ctxt->a_path.src_data_eos;
        is_enough = &ctxt->a_path.src_data_enough;
        max_ts = &ctxt->a_path.max_ts;
        min = &ctxt->data_ctl.min_a;
        max = &ctxt->data_ctl.max_a;
        break;
    };
    default:
    {
        GST_ERROR("error type");
        goto done;
    }
    }

    if (!(*is_exist))
    {
        GST_INFO("path(type:%d) is not exist.", type);
        status = AGMP_ES_DATA_BUFFERING_DONE;
        goto done;
    }
    if (GST_CLOCK_TIME_NONE == *max_ts)
    {
        GST_INFO("path(type:%d) is prerolling.", type);
        status = AGMP_ES_DATA_BUFFERING_DONE;
        goto done;
    }
    if (*is_eos)
    {
        GST_INFO("path(type:%d) is eos.", type);
        status = AGMP_ES_DATA_BUFFERING_DONE;
        goto done;
    }
    if (*is_enough)
    {
        GST_INFO("path(type:%d) is enought based on appsrc's data level.", type);
        status = AGMP_ES_DATA_BUFFERING_DONE;
        goto done;
    }
    if ((position = _agmp_es_get_position(ctxt)) == GST_CLOCK_TIME_NONE)
    {
        GST_INFO("cur positon is GST_CLOCK_TIME_NONE");
        status = AGMP_ES_DATA_BUFFERING_DONE;
        goto done;
    }

    status = AGMP_ES_DATA_NEED_NORMAL;
    if (*max_ts < position || (*max_ts - position) < ((*min) * GST_MSECOND))
        status = AGMP_ES_DATA_NEED_BUFFERING;
    else if ((*max_ts - position) > ((*max) * GST_MSECOND))
        status = AGMP_ES_DATA_BUFFERING_DONE;

    GST_INFO("type:%d status:%d"
             " render %" GST_TIME_FORMAT " max %" GST_TIME_FORMAT " diff %" GST_TIME_FORMAT
             " low %" GST_TIME_FORMAT " high %" GST_TIME_FORMAT,
             type, status,
             GST_TIME_ARGS(position), GST_TIME_ARGS(*max_ts), GST_TIME_ARGS(*max_ts - position),
             GST_TIME_ARGS((*min) * GST_MSECOND), GST_TIME_ARGS((*max) * GST_MSECOND));
done:
    GST_INFO("path(type:%d) cur data status is:%d.", type, status);
    GST_TRACE("trace out ret AgmpEsDataStatus:%d", status);
    return status;
}

void _agmp_es_data_clear_status(AgmpEsCtxt *ctxt)
{
    GST_TRACE("trace in");

    if (ctxt->v_path.exist)
    {
        ctxt->v_path.src_data_enough = FALSE;
        ctxt->v_path.src_data_eos = FALSE;
        ctxt->v_path.max_ts = GST_CLOCK_TIME_NONE;
        ctxt->v_path.dropped_frame_num = 0;
        g_atomic_int_set(&ctxt->v_path.data_waiting, 0);
    }

    if (ctxt->a_path.exist)
    {
        ctxt->a_path.src_data_enough = FALSE;
        ctxt->a_path.src_data_eos = FALSE;
        ctxt->a_path.max_ts = GST_CLOCK_TIME_NONE;
        g_atomic_int_set(&ctxt->a_path.data_waiting, 0);
    }

    GST_TRACE("trace out");
}

GstClockTime _agmp_es_get_position(AgmpEsCtxt *ctxt)
{
    AgmpEsStateType state;
    gint64 pos;

    GST_TRACE("trace in");

    if ((GST_STATE(ctxt->pipeline) < GST_STATE_PAUSED))
    {
        pos = GST_CLOCK_TIME_NONE;
        GST_INFO("player in init/deinit flow pos %" GST_TIME_FORMAT, GST_TIME_ARGS(pos));
        goto done;
    }

    state = agmp_es_get_state((AGMP_ES_HANDLE)ctxt);
    if (AGMP_ES_STATE_PREROLL_INIT == state || AGMP_ES_STATE_PREROLL_AFTER_SEEK == state)
    {
        pos = ctxt->seek_to_pos;
        GST_INFO("player in preroll flow pos %" GST_TIME_FORMAT, GST_TIME_ARGS(pos));
        goto done;
    }

    GstQuery *query = gst_query_new_position(GST_FORMAT_TIME);
    if (gst_element_query(ctxt->pipeline, query))
    {
        gst_query_parse_position(query, 0, &pos);
        GST_INFO("query postion succ. pos %" GST_TIME_FORMAT, GST_TIME_ARGS(pos));
    }
    else
    {
        pos = GST_CLOCK_TIME_NONE;
        GST_ERROR("query postion fail. pos %" GST_TIME_FORMAT, GST_TIME_ARGS(pos));
    }
    gst_query_unref(query);

done:
    GST_TRACE("trace out");
    return pos;
}

/* static inline functions */
inline gboolean _agmp_es_data_has_enough(AgmpEsCtxt *ctxt)
{
    return _agmp_es_data_v_enough(ctxt) || _agmp_es_data_a_enough(ctxt);
}

inline gboolean _agmp_es_data_all_enough(AgmpEsCtxt *ctxt)
{
    return _agmp_es_data_v_enough(ctxt) && _agmp_es_data_a_enough(ctxt);
}

inline gboolean _agmp_es_data_v_enough(AgmpEsCtxt *ctxt)
{
    gboolean ret;

    ret = FALSE;

    ret |= !ctxt->v_path.exist;                             // if video does not exist, it is always enough
    ret |= ctxt->v_path.exist && ctxt->v_path.src_data_eos; // if video rreceived eos, it is enough
    ret |= ctxt->v_path.exist && ctxt->v_path.src_data_enough;

    return ret;
}

inline gboolean _agmp_es_data_a_enough(AgmpEsCtxt *ctxt)
{
    gboolean ret;

    ret = FALSE;

    ret |= !ctxt->a_path.exist;                             // if audio does not exist, it is always enough
    ret |= ctxt->a_path.exist && ctxt->a_path.src_data_eos; // if audio rreceived eos, it is enough
    ret |= ctxt->a_path.exist && ctxt->a_path.src_data_enough;

    return ret;
}

inline gboolean _agmp_es_data_has_eos(AgmpEsCtxt *ctxt)
{
    return _agmp_es_data_v_eos(ctxt) || _agmp_es_data_a_eos(ctxt);
}

inline gboolean _agmp_es_data_all_eos(AgmpEsCtxt *ctxt)
{
    return _agmp_es_data_v_eos(ctxt) && _agmp_es_data_a_eos(ctxt);
}

inline gboolean _agmp_es_data_v_eos(AgmpEsCtxt *ctxt)
{
    return (ctxt->v_path.exist && ctxt->v_path.src_data_eos) || !ctxt->v_path.exist;
}

inline gboolean _agmp_es_data_a_eos(AgmpEsCtxt *ctxt)
{
    return (ctxt->a_path.exist && ctxt->a_path.src_data_eos) || !ctxt->a_path.exist;
}

inline gboolean _agmp_es_is_data_msg(AgmpMsgType type)
{
    return type >= AGMP_MSG_DATA_NEED && type <= AGMP_MSG_DATA_STAT_HIGH;
}

inline gboolean _agmp_es_is_state_msg(AgmpMsgType type)
{
    return type >= AGMP_MSG_STATE_INIT && type <= AGMP_MSG_STATE_DESTROY;
}
