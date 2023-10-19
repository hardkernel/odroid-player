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

#ifndef __AGMPLAYER_ES_TYPES_H__
#define __AGMPLAYER_ES_TYPES_H__

#include <stdio.h>
#include <float.h>
#include <stdint.h>

#include "agmplayer_es_commons.h"

typedef enum AgmpEsType
{
    AGMP_NONE,
    AGMP_VID,
    AGMP_AUD,
} AgmpEsType;

typedef enum AgmpMsgType
{
    AGMP_MSG_STATE_INIT,
    AGMP_MSG_STATE_PREROLL,
    AGMP_MSG_STATE_PRESENT,
    AGMP_MSG_STATE_EOS,
    AGMP_MSG_STATE_DESTROY,

    AGMP_MSG_DATA_NEED,      // for appsrc need data
    AGMP_MSG_DATA_ENOUGH,    // for appsrc data enough
    AGMP_MSG_DATA_RELEASE,   // for release upper-layer data
    AGMP_MSG_DATA_STAT_LOW,  // indicate low watermark， agmp-es will pause pipeline to acquire more buffer
    AGMP_MSG_DATA_STAT_HIGH, // indicate high watermark， agmp-es will resume from internal pause

    AGMP_MSG_STATUS_UPDATE, // indicate agmp-es still alive

    AGMP_MSG_ERROR_DEC,
    AGMP_MSG_ERROR_CAP_CHG,
} AgmpMsgType;

typedef enum AgmpEsStateType
{
    AGMP_ES_STATE_NULL,
    AGMP_ES_STATE_INIT,
    AGMP_ES_STATE_PREROLL_INIT,
    AGMP_ES_STATE_PREROLL_AFTER_SEEK,
    AGMP_ES_STATE_PRESENT,
    AGMP_ES_STATE_EOS,
    AGMP_ES_STATE_DESTROY,
} AgmpEsStateType;

typedef enum AgmpVidCodecType
{
    VCODEC_NONE,
    VCODEC_H264,
    VCODEC_H265,
    VCODEC_MPEG2,
    VCODEC_THEORA,
    VCODEC_VC1,
    VCODEC_AV1,
    VCODEC_VP8,
    VCODEC_VP9,
} AgmpVidCodecType;

typedef enum AgmpAudCodecType
{
    ACODEC_NONE,
    ACODEC_AAC,
    ACODEC_AC3,
    ACODEC_EAC3,
    ACODEC_OPUS,
    ACODEC_VORBIS,
    ACODEC_MP3,
    ACODEC_FLAC,
    ACODEC_PCM,
} AgmpAudCodecType;

#endif /* __AGMPLAYER_ES_TYPES_H__ */
