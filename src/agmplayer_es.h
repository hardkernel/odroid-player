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

#ifndef __AGMPLAYER_ES_H__
#define __AGMPLAYER_ES_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdio.h>
#include <float.h>
#include<stdint.h>

#include "agmplayer_es_commons.h"
#include "agmplayer_es_types.h"
#include "agmplayer_es_cfgs.h"
#include "agmplayer_es_msgs.h"
#include "agmplayer_es_infos.h"
#include "agmplayer_es_video_color_metadata.h"

AGMP_ES_HANDLE agmp_es_create(AgmpEsCfg *cfg);
void agmp_es_destroy(AGMP_ES_HANDLE handle);

BOOL agmp_es_acquire_cfgs(AGMP_ES_HANDLE handle, AgmpEsCfg *cfg);

BOOL agmp_es_start(AGMP_ES_HANDLE handle);
BOOL agmp_es_stop(AGMP_ES_HANDLE handle);

BOOL agmp_es_update_format(AGMP_ES_HANDLE handle, AgmpFormatInfo *info);

void agmp_es_set_display_window(AGMP_ES_HANDLE handle, AgmpWindow *window);
void agmp_es_set_volume(AGMP_ES_HANDLE handle, double volume);
BOOL agmp_es_set_rate(AGMP_ES_HANDLE handle, double rate);
BOOL agmp_es_set_pause(AGMP_ES_HANDLE handle);
BOOL agmp_es_set_play(AGMP_ES_HANDLE handle);
BOOL agmp_es_set_eos(AGMP_ES_HANDLE handle, AgmpEsType type);

BOOL agmp_es_write(AGMP_ES_HANDLE handle, AgmpDataInfo *data_info);
/*
    description:
        use this func do seek
    params:
        handle: agmp-es handle
        rate:the rendering rate after seek done
        pos: seek to postion in ms
*/
BOOL agmp_es_seek(AGMP_ES_HANDLE handle, double rate, int64_t pos);

AgmpEsStateType agmp_es_get_state(AGMP_ES_HANDLE handle);
BOOL agmp_es_get_play_info(AGMP_ES_HANDLE handle, AgmpPlayInfo *play_info);

int64_t agmp_es_data_get_time_level(AGMP_ES_HANDLE handle, AgmpEsType type);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __AGMPLAYER_ES_H__ */