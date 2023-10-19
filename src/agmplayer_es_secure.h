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

#ifndef __AGMPLAYER_ES_SECURE_H__
#define __AGMPLAYER_ES_SECURE_H__

#include <stdio.h>
#include <float.h>
#include <stdint.h>

#include "agmplayer_es.h"

GST_DEBUG_CATEGORY_EXTERN(agmp_es_debug);
#define GST_CAT_DEFAULT agmp_es_debug

typedef struct _AgmpEsSecCtxt AgmpEsSecCtxt;

AgmpEsSecCtxt *agmp_es_sec_create(uint8_t format, gboolean is_4k);
void agmp_es_sec_destroy(AgmpEsSecCtxt *sec_ctxt);
void agmp_es_sec_start_polling_res_available(AgmpEsSecCtxt *sec_ctxt);
gboolean agmp_es_sec_check_mem_available(AgmpEsSecCtxt *sec_ctxt);
gboolean agmp_es_sec_res_available(AgmpEsSecCtxt *sec_ctxt);

#endif /* __AGMPLAYER_ES_SECURE_H__ */
