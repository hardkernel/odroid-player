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

#ifndef __AGMPLAYER_ES_CFGS_H__
#define __AGMPLAYER_ES_CFGS_H__

#include <stdio.h>
#include <float.h>
#include <stdint.h>

#include "agmplayer_es_commons.h"
#include "agmplayer_es_infos.h"
#include "agmplayer_es_msgs.h"

typedef struct _AgmpEsCfg AgmpEsCfg;
typedef struct _AgmpEsCommonCfg AgmpEsCommonCfg;
typedef struct _AgmpEsVidCfg AgmpEsVidCfg;
typedef struct _AgmpEsAudCfg AgmpEsAudCfg;

/* cfgs struct */
struct _AgmpEsCommonCfg
{
    /*
        agmp-es will create a pipeline render video in sub video layer and mute audio if pip enable.
        default 0 for disable.
    */
    BOOL pip_mode; // default 0 for disable

    /*
        upper layer will inject secure es data.
        default 0 for disable.
    */
    BOOL secure_mode;

    /*
        agmp-es will create new AGMP_MSG_DATA_NEED msg only after
        receive a new data (which is in response to last AGMP_MSG_DATA_NEED msg)
        from upper layer if serial data enable.
        default 0 for disable.
    */
    BOOL serial_data_mode;

    /*
        for cobalt watch dog hang_monitor.
        default 0 for disable
    */
    int64_t status_update_interval;

    /*
        player usr data
        opaque for agmp-es
    */
    void *user_data;
    /*
        message callback func
        can't be null
    */
    void (*msg_cb)(void *user_data, AgmpMsg *msg);

    /*
        external decryption func
    */
    BOOL (*decrypt)
    (void *user_data, AgmpEsType type,
     AgmpDrmEncScheme enc_scheme,
     uint32_t crypt_block_cnt, uint32_t skip_block_cnt,
     uint8_t *data, uint32_t size,
     uint8_t *key_data, uint32_t key_data_size,
     uint8_t *iv_data, uint32_t iv_data_size,
     uint8_t *subsamples_data, uint32_t subsamples_data_size, uint32_t subsample_cnt,
     uint32_t handle);
};

struct _AgmpEsVidCfg
{
    /*
        video codec type
    */
    AgmpVidCodecType vcodec;

    /*
        agmp-es will send AGMP_MSG_DATA_ENOUGH msg when it's queue reach this size.
        agmp-es will resume and send AGMP_MSG_DATA_STAT_HIGH msg if pause internal.
        0 means infinite.
    */
    int src_max_byte_size;

    /*
        agmp-es will send AGMP_MSG_DATA_NEED msg when it's queue's bytes level below this percent.
    */
    int src_min_percent;

    /*
        agmp-es will send AGMP_MSG_DATA_STAT_LOW msg when it's queue's bytes level below this percent
        and enter internal pause.
    */
    int src_low_percent;

    /*
        display window
    */
    AgmpWindow disp_window;

    /*
        default 0 for disable
    */
    BOOL low_mem_mode;

    /*
        video format info
    */
    AgmpVidFormatInfo format_info;
};

struct _AgmpEsAudCfg
{
    /*
        audio codec type
    */
    AgmpAudCodecType acodec;

    /*
        agmp-es will send AGMP_MSG_DATA_ENOUGH msg when it's queue reach this size.
        agmp-es will resume and send AGMP_MSG_DATA_STAT_HIGH msg if pause internal
        0 means infinite
    */
    int src_max_byte_size;

    /*
        agmp-es will send AGMP_MSG_DATA_NEED msg when it's queue's bytes level below this percent
    */
    int src_min_percent;

    /*
        agmp-es will send AGMP_MSG_DATA_STAT_LOW msg when it's queue's bytes level below this percent
        and enter internal pause
    */
    int src_low_percent;

    /*
        audio format info
    */
    AgmpAudFormatInfo format_info;
};

struct _AgmpEsCfg
{
    AgmpEsCommonCfg common_cfgs; // agmp-es common configs
    AgmpEsVidCfg vid_cfgs;       // agmp-es video configs
    AgmpEsAudCfg aud_cfgs;       // agmp-es audio configs
};

#endif /* __AGMPLAYER_ES_CFGS_H__ */
