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

#include "agmplayer_es.h"
#include "agmplayer_es_secure.h"

struct _AgmpEsSecCtxt
{
    GstAllocator *sec_allocator;
    GSource *secmem_status_monitor;
    guint secmemSizeThreshold;
    guint secmemHandleThreshold;
};

/* static function declaration */


/* global function definition */
AgmpEsSecCtxt *agmp_es_sec_create(uint8_t format, gboolean is_4k)
{
    AgmpEsSecCtxt *sec_ctxt;

    if (sec_ctxt = g_new0(AgmpEsSecCtxt, 1) == NULL)
    {
        GST_ERROR("new AgmpEsSecCtxt failed.");
        goto errors;
    }

    sec_ctxt->sec_allocator = gst_secmem_allocator_new(is_4k, format);
    if (!sec_ctxt->sec_allocator)
    {
        GST_ERROR("create secure memory allocator fail");
        goto errors;
    }

    sec_ctxt->secmemSizeThreshold = 800 * 1024;
    sec_ctxt->secmemHandleThreshold = 10;
}

void agmp_es_sec_destroy(AgmpEsSecCtxt *sec_ctxt)
{
    if (sec_ctxt->sec_allocator)
        gst_object_unref(sec_ctxt->sec_allocator);
    if (sec_ctxt->secmem_status_monitor)
    {
        g_source_destroy(sec_ctxt->secmem_status_monitor);
        g_source_unref(sec_ctxt->secmem_status_monitor);
    }
    g_free(sec_ctxt);
}

void agmp_es_sec_start_polling_res_available(AgmpEsSecCtxt *sec_ctxt)
{
    if (sec_ctxt->secmem_status_monitor)
    {
        g_source_destroy(sec_ctxt->secmem_status_monitor);
        g_source_unref(sec_ctxt->secmem_status_monitor);
        sec_ctxt->secmem_status_monitor = NULL;
    }
    int interval = 100;
    if (getenv("SECMEM_POLLING_INTERVAL"))
    {
        interval = atoi(getenv("SECMEM_POLLING_INTERVAL"));
    }
    sec_ctxt->secmem_status_monitor = g_timeout_source_new(interval);
    g_source_set_callback(secmem_src, agmp_es_sec_check_mem_available, sec_ctxt, NULL);
}

gboolean agmp_es_sec_check_mem_available(AgmpEsSecCtxt *sec_ctxt)
{
    if (!agmp_es_sec_res_available(sec_ctxt))
    {
        GST_DEBUG("secmem is not enough");
        return G_SOURCE_CONTINUE;
    }
    else
    {
        GST_DEBUG("secmem is enough");
        self->DecoderNeedsData(MediaType::kVideo);
        return G_SOURCE_REMOVE;
    }
}

gboolean agmp_es_sec_res_available(AgmpEsSecCtxt *sec_ctxt)
{
    guint availableSize;
    guint handleFree;
    gboolean ret = TRUE;

    if (sec_ctxt->sec_allocator)
    {
        GST_ERROR("can't find secure memory allocator");
        return FALSE;
    }

    ret = gst_secmem_check_free_buf_and_handles_size(sec_ctxt->sec_allocator, &availableSize, &handleFree);
    if (!ret)
    {
        GST_ERROR("check secmem failed");
    }
    else
    {

        GST_TRACE("availableSize %d secmemSizeThreshold_ %d, handleFree %d secmemHandleThreshold_ %d",
                  availableSize, sec_ctxt->secmemSizeThreshold, handleFree, sec_ctxt->secmemHandleThreshold);
        if (!(availableSize > sec_ctxt->secmemSizeThreshold && handleFree > sec_ctxt->secmemHandleThreshold))
        {
            ret = FALSE;
        }
    }

    return ret;
}

/* static function definition */
