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
#include <gst/video/video.h>

#include "agmplayer_es_video_color_metadata.h"
#include "agmplayer_es_video_color_metadata_internal.h"

static GstVideoColorRange RangeIdToGstVideoColorRange(AgmpMediaRangeId value);
static GstVideoColorMatrix MatrixIdToGstVideoColorMatrix(AgmpMediaMatrixId value);
static GstVideoTransferFunction TransferIdToGstVideoTransferFunction(AgmpMediaTransferId value);
static GstVideoColorPrimaries PrimaryIdToGstVideoColorPrimaries(AgmpMediaPrimaryId value);

GstVideoColorRange RangeIdToGstVideoColorRange(AgmpMediaRangeId value)
{
    switch (value)
    {
    case agmpMediaRangeIdLimited:
        return GST_VIDEO_COLOR_RANGE_16_235;
    case agmpMediaRangeIdFull:
        return GST_VIDEO_COLOR_RANGE_0_255;
    default:
    case agmpMediaRangeIdUnspecified:
        return GST_VIDEO_COLOR_RANGE_UNKNOWN;
    }
}

GstVideoColorMatrix MatrixIdToGstVideoColorMatrix(AgmpMediaMatrixId value)
{
    switch (value)
    {
    case agmpMediaMatrixIdRgb:
        return GST_VIDEO_COLOR_MATRIX_RGB;
    case agmpMediaMatrixIdBt709:
        return GST_VIDEO_COLOR_MATRIX_BT709;
    case agmpMediaMatrixIdFcc:
        return GST_VIDEO_COLOR_MATRIX_FCC;
    case agmpMediaMatrixIdBt470Bg:
    case agmpMediaMatrixIdSmpte170M:
        return GST_VIDEO_COLOR_MATRIX_BT601;
    case agmpMediaMatrixIdSmpte240M:
        return GST_VIDEO_COLOR_MATRIX_SMPTE240M;
    case agmpMediaMatrixIdBt2020NonconstantLuminance:
        return GST_VIDEO_COLOR_MATRIX_BT2020;
    case agmpMediaMatrixIdUnspecified:
    default:
        return GST_VIDEO_COLOR_MATRIX_UNKNOWN;
    }
}

GstVideoTransferFunction TransferIdToGstVideoTransferFunction(AgmpMediaTransferId value)
{
    switch (value)
    {
    case agmpMediaTransferIdBt709:
    case agmpMediaTransferIdSmpte170M:
        return GST_VIDEO_TRANSFER_BT709;
    case agmpMediaTransferIdGamma22:
        return GST_VIDEO_TRANSFER_GAMMA22;
    case agmpMediaTransferIdGamma28:
        return GST_VIDEO_TRANSFER_GAMMA28;
    case agmpMediaTransferIdSmpte240M:
        return GST_VIDEO_TRANSFER_SMPTE240M;
    case agmpMediaTransferIdLinear:
        return GST_VIDEO_TRANSFER_GAMMA10;
    case agmpMediaTransferIdLog:
        return GST_VIDEO_TRANSFER_LOG100;
    case agmpMediaTransferIdLogSqrt:
        return GST_VIDEO_TRANSFER_LOG316;
    case agmpMediaTransferIdIec6196621:
        return GST_VIDEO_TRANSFER_SRGB;
    case agmpMediaTransferId10BitBt2020:
        return GST_VIDEO_TRANSFER_BT2020_10;
    case agmpMediaTransferId12BitBt2020:
        return GST_VIDEO_TRANSFER_BT2020_12;
    case agmpMediaTransferIdSmpteSt2084:
#if GST_CHECK_VERSION(1, 18, 0)
        return GST_VIDEO_TRANSFER_SMPTE2084;
#else
        return GST_VIDEO_TRANSFER_SMPTE_ST_2084;
#endif
    case agmpMediaTransferIdAribStdB67:
        return GST_VIDEO_TRANSFER_ARIB_STD_B67;
    case agmpMediaTransferIdUnspecified:
    default:
        return GST_VIDEO_TRANSFER_UNKNOWN;
    }
}

GstVideoColorPrimaries PrimaryIdToGstVideoColorPrimaries(AgmpMediaPrimaryId value)
{
    switch (value)
    {
    case agmpMediaPrimaryIdBt709:
        return GST_VIDEO_COLOR_PRIMARIES_BT709;
    case agmpMediaPrimaryIdBt470M:
        return GST_VIDEO_COLOR_PRIMARIES_BT470M;
    case agmpMediaPrimaryIdBt470Bg:
        return GST_VIDEO_COLOR_PRIMARIES_BT470BG;
    case agmpMediaPrimaryIdSmpte170M:
        return GST_VIDEO_COLOR_PRIMARIES_SMPTE170M;
    case agmpMediaPrimaryIdSmpte240M:
        return GST_VIDEO_COLOR_PRIMARIES_SMPTE240M;
    case agmpMediaPrimaryIdFilm:
        return GST_VIDEO_COLOR_PRIMARIES_FILM;
    case agmpMediaPrimaryIdBt2020:
        return GST_VIDEO_COLOR_PRIMARIES_BT2020;
    case agmpMediaPrimaryIdUnspecified:
    default:
        return GST_VIDEO_COLOR_PRIMARIES_UNKNOWN;
    }
}

void _agmp_es_update_vid_colormeta_into_caps(GstCaps *caps, AgmpMediaColorMetadata *color_metadata)
{
    GstVideoColorimetry colorimetry;

    colorimetry.range = RangeIdToGstVideoColorRange(color_metadata->range);
    colorimetry.matrix = MatrixIdToGstVideoColorMatrix(color_metadata->matrix);
    colorimetry.transfer = TransferIdToGstVideoTransferFunction(color_metadata->transfer);
    colorimetry.primaries = PrimaryIdToGstVideoColorPrimaries(color_metadata->primaries);

    if (colorimetry.range != GST_VIDEO_COLOR_RANGE_UNKNOWN ||
        colorimetry.matrix != GST_VIDEO_COLOR_MATRIX_UNKNOWN ||
        colorimetry.transfer != GST_VIDEO_TRANSFER_UNKNOWN ||
        colorimetry.primaries != GST_VIDEO_COLOR_PRIMARIES_UNKNOWN)
    {
        gchar *tmp = gst_video_colorimetry_to_string(&colorimetry);
        gst_caps_set_simple(caps, "colorimetry", G_TYPE_STRING, tmp, NULL);
        GST_DEBUG("Setting \"colorimetry\" to %s", tmp);
        g_free(tmp);
    }
#if GST_CHECK_VERSION(1, 18, 0)
    GstVideoMasteringDisplayInfo mastering_display_info;
    gst_video_mastering_display_info_init(&mastering_display_info); //  gst_video_mastering_display_metadata_init (&mastering_display_metadata);

    mastering_display_info.display_primaries[0].x = (guint16)(color_metadata->mastering_metadata.primary_r_chromaticity_x * 50000);
    mastering_display_info.display_primaries[0].y = (guint16)(color_metadata->mastering_metadata.primary_r_chromaticity_y * 50000);
    mastering_display_info.display_primaries[1].x = (guint16)(color_metadata->mastering_metadata.primary_g_chromaticity_x * 50000);
    mastering_display_info.display_primaries[1].y = (guint16)(color_metadata->mastering_metadata.primary_g_chromaticity_y * 50000);
    mastering_display_info.display_primaries[2].x = (guint16)(color_metadata->mastering_metadata.primary_b_chromaticity_x * 50000);
    mastering_display_info.display_primaries[2].y = (guint16)(color_metadata->mastering_metadata.primary_b_chromaticity_y * 50000);
    mastering_display_info.white_point.x = (guint16)(color_metadata->mastering_metadata.white_point_chromaticity_x * 50000);
    mastering_display_info.white_point.y = (guint16)(color_metadata->mastering_metadata.white_point_chromaticity_y * 50000);
    mastering_display_info.max_display_mastering_luminance = (guint32)ceil(color_metadata->mastering_metadata.luminance_max);
    mastering_display_info.min_display_mastering_luminance = (guint32)ceil(color_metadata->mastering_metadata.luminance_min);

    gchar *tmp = gst_video_mastering_display_info_to_string(&mastering_display_info);
    gst_caps_set_simple(caps, "mastering-display-info", G_TYPE_STRING, tmp, NULL);
    GST_DEBUG("Setting \"mastering-display-info\" to %s", tmp);
    g_free(tmp);
#else
    GstVideoMasteringDisplayMetadata mastering_display_metadata;
    gst_video_mastering_display_metadata_init(&mastering_display_metadata);
    mastering_display_metadata.Rx = color_metadata->mastering_metadata.primary_r_chromaticity_x;
    mastering_display_metadata.Ry = color_metadata->mastering_metadata.primary_r_chromaticity_y;
    mastering_display_metadata.Gx = color_metadata->mastering_metadata.primary_g_chromaticity_x;
    mastering_display_metadata.Gy = color_metadata->mastering_metadata.primary_g_chromaticity_y;
    mastering_display_metadata.Bx = color_metadata->mastering_metadata.primary_b_chromaticity_x;
    mastering_display_metadata.By = color_metadata->mastering_metadata.primary_b_chromaticity_y;
    mastering_display_metadata.Wx = color_metadata->mastering_metadata.white_point_chromaticity_x;
    mastering_display_metadata.Wy = color_metadata->mastering_metadata.white_point_chromaticity_y;
    mastering_display_metadata.max_luma = color_metadata->mastering_metadata.luminance_max;
    mastering_display_metadata.min_luma = color_metadata->mastering_metadata.luminance_min;

    if (gst_video_mastering_display_metadata_has_primaries(&mastering_display_metadata) &&
        gst_video_mastering_display_metadata_has_luminance(&mastering_display_metadata))
    {
        gchar *tmp = gst_video_mastering_display_metadata_to_caps_string(&mastering_display_metadata);
        gst_caps_set_simple(caps, "mastering-display-metadata", G_TYPE_STRING, tmp, NULL);
        GST_DEBUG("Setting \"mastering-display-metadata\" to %s", tmp);
        g_free(tmp);
    }
#endif
    if (color_metadata->max_cll && color_metadata->max_fall)
    {
        GstVideoContentLightLevel content_light_level;
#if GST_CHECK_VERSION(1, 18, 0)
        content_light_level.max_content_light_level = color_metadata->max_cll;
        content_light_level.max_frame_average_light_level = color_metadata->max_fall;
        gchar *tmp = gst_video_content_light_level_to_string(&content_light_level);
#else
        content_light_level.maxCLL = color_metadata->max_cll;
        content_light_level.maxFALL = color_metadata->max_fall;
        gchar *tmp = gst_video_content_light_level_to_caps_string(&content_light_level);
#endif
        gst_caps_set_simple(caps, "content-light-level", G_TYPE_STRING, tmp, NULL);
        GST_DEBUG("setting \"content-light-level\" to %s", tmp);
        g_free(tmp);
    }
}