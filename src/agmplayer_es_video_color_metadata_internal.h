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

#ifndef __AGMPLAYER_ES_VIDEO_COLOR_METADATA_INTERNAL_H__
#define __AGMPLAYER_ES_VIDEO_COLOR_METADATA_INTERNAL_H__

#include <stdio.h>
#include <float.h>
#include <stdint.h>

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/pbutils/pbutils.h>
#include <gst/tag/tag.h>
#include <gst/math-compat.h>

#include "agmplayer_es_video_color_metadata.h"

void _agmp_es_update_vid_colormeta_into_caps(GstCaps *caps, AgmpMediaColorMetadata *color_metadata);

#endif /* __AGMPLAYER_ES_VIDEO_COLOR_METADATA_INTERNAL_H__ */
