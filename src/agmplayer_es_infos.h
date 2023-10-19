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

#ifndef __AGMPLAYER_ES_CFGS_INFOS_H__
#define __AGMPLAYER_ES_CFGS_INFOS_H__

#include <stdio.h>
#include <float.h>
#include <stdint.h>

#include "agmplayer_es_commons.h"
#include "agmplayer_es_video_color_metadata.h"

#define AGMP_ES_AUD_SPEC_DATA_MAX_SIZE 32

typedef struct _AgmpFormatInfo AgmpFormatInfo;
typedef struct _AgmpVidFormatInfo AgmpVidFormatInfo;
typedef struct _AgmpAudFormatInfo AgmpAudFormatInfo;
typedef struct _AgmpEsAudSpecData AgmpEsAudSpecData;

typedef struct _AgmpDataInfo AgmpDataInfo;
typedef struct _AgmpVidDataInfo AgmpVidDataInfo;
typedef struct _AgmpAudDataInfo AgmpAudDataInfo;
typedef struct _AgmpDrmDataInfo AgmpDrmDataInfo;
typedef struct _AgmpDrmEncPattern AgmpDrmEncPattern;
typedef struct _AgmpDrmSubSampleMapping AgmpDrmSubSampleMapping;

typedef struct _AgmpPlayInfo AgmpPlayInfo;

/* format infos */
struct _AgmpVidFormatInfo
{
    int frame_width;
    int frame_height;
    BOOL has_color_metadata;
    AgmpMediaColorMetadata color_metadata;
};

struct _AgmpEsAudSpecData
{
    gchar data[AGMP_ES_AUD_SPEC_DATA_MAX_SIZE];
    int size;
};

struct _AgmpAudFormatInfo
{
    int number_of_channels;
    int samples_per_second;
    BOOL has_specdata;
    AgmpEsAudSpecData data; // audio special data
};

struct _AgmpFormatInfo
{
    AgmpEsType type;
    union
    {
        AgmpVidFormatInfo vinfo;
        AgmpAudFormatInfo ainfo;
    } u;
};

/* data infos */
/*
    Encryption scheme of the input sample, as defined in ISO/IEC 23001 part 7.
*/
typedef enum AgmpDrmEncScheme
{
    AGMP_ENC_SCHEME_NONE,
    AGMP_ENC_SCHEME_AESCTR,
    AGMP_ENC_SCHEME_AESCBC,
} AgmpDrmEncScheme;

/*
    Encryption scheme of the input sample, as defined in ISO/IEC 23001 part 7.
*/
struct _AgmpDrmEncPattern
{
    /*
        specifies the count of the encrypted Blocks in the protection pattern,
        where each Block is of size 16-bytes
     */
    uint32_t crypt_byte_block;
    /*
         specifies the count of the unencrypted Blocks in the protection pattern
     */
    uint32_t skip_byte_block;
};

/*
    A mapping of clear and encrypted bytes for a single subsample.
    All subsamples within a sample must be encrypted with the same encryption parameters.
    The clear bytes always appear first in the sample.
*/
struct _AgmpDrmSubSampleMapping
{
    /*
        How many bytes of the corresponding subsample are not encrypted
    */
    int32_t clear_byte_count;

    /*
        How many bytes of the corresponding subsample are encrypted.
    */
    int32_t encrypted_byte_count;
};

/*
    All the optional information needed per sample for encrypted samples.
*/
struct _AgmpDrmDataInfo
{
    BOOL exist;
    /*
        The encryption scheme of this sample.
    */
    AgmpDrmEncScheme enc_scheme;

    /*
        The encryption pattern of this sample.
    */
    AgmpDrmEncPattern enc_pattern;

    /*
        The Initialization Vector needed to decrypt this sample.
    */
    uint8_t iv[16];
    int iv_size;

    /*
        The ID of the license (or key) required to decrypt this sample.
        For PlayReady, this is the license GUID in packed little-endian binary form.
    */
    uint8_t id[16];
    int id_size;

    /*
        The number of subsamples in this sample, must be at least 1.
    */
    int32_t subsample_count;

    /*
        The clear/encrypted mapping of each subsample in this sample.
        This must be an array of |subsample_count| mappings.
        subsample_mapping is an array and array len is subsample_count.
    */
    AgmpDrmSubSampleMapping *subsample_mapping;
};

struct _AgmpVidDataInfo
{
    BOOL keyframe;
};

struct _AgmpAudDataInfo
{
};

struct _AgmpDataInfo
{
    AgmpEsType type;
    int64_t timestamp; // ns
    const void *data;
    int size;
    union
    {
        AgmpVidDataInfo vinfo;
        AgmpAudDataInfo ainfo;
    } u;
    AgmpDrmDataInfo drm_info;

    void *usr_data;
};

/* play infos */
struct _AgmpPlayInfo
{
    gint64 duration; // ms
    gint64 position; // ms

    int frame_width;
    int frame_height;

    BOOL is_paused;

    double volume;
    double playback_rate;

    int total_video_frames;
    int dropped_video_frames;
    int corrupted_video_frames;
};

#endif /* __AGMPLAYER_ES_CFGS_INFOS_H__ */
