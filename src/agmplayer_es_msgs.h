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

#ifndef __AGMPLAYER_ES_MSGS_H__
#define __AGMPLAYER_ES_MSGS_H__

#include <stdio.h>
#include <float.h>
#include <stdint.h>

#include "agmplayer_es_commons.h"
#include "agmplayer_es_types.h"

typedef struct _AgmpMsg AgmpMsg;
typedef struct _AgmpMsgBodyDataState AgmpMsgBodyDataState;

struct _AgmpMsgBodyDataState
{
    AgmpEsType type;
    void *usr_data;
};

struct _AgmpMsg
{
    void *agmp_handle;
    AgmpMsgType type;
    int64_t send_time;       // in us
    int64_t Scheduling_time; // in us
    int64_t finish_time;     // in us
    union
    {
        AgmpMsgBodyDataState data_state;
    } body;
};

#endif /* __AGMPLAYER_ES_MSGS_H__ */
