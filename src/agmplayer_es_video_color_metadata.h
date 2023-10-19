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

#ifndef __AGMPLAYER_ES_VIDEO_COLOR_METADATA_H__
#define __AGMPLAYER_ES_VIDEO_COLOR_METADATA_H__

#include <stdio.h>
#include <float.h>
#include <stdint.h>

typedef struct _AgmpMediaColorMetadata AgmpMediaColorMetadata;
typedef struct _AgmpMediaMasteringMetadata AgmpMediaMasteringMetadata;

typedef enum AgmpMediaPrimaryId
{
    // The first 0-255 values should match the H264 specification (see Table E-3
    // Colour Primaries in https://www.itu.int/rec/T-REC-H.264/en).
    agmpMediaPrimaryIdReserved0 = 0,
    agmpMediaPrimaryIdBt709 = 1,
    agmpMediaPrimaryIdUnspecified = 2,
    agmpMediaPrimaryIdReserved = 3,
    agmpMediaPrimaryIdBt470M = 4,
    agmpMediaPrimaryIdBt470Bg = 5,
    agmpMediaPrimaryIdSmpte170M = 6,
    agmpMediaPrimaryIdSmpte240M = 7,
    agmpMediaPrimaryIdFilm = 8,
    agmpMediaPrimaryIdBt2020 = 9,
    agmpMediaPrimaryIdSmpteSt4281 = 10,
    agmpMediaPrimaryIdSmpteSt4312 = 11,
    agmpMediaPrimaryIdSmpteSt4321 = 12,

    agmpMediaPrimaryIdLastStandardValue = agmpMediaPrimaryIdSmpteSt4321,

    // Chrome-specific values start at 1000.
    agmpMediaPrimaryIdUnknown = 1000,
    agmpMediaPrimaryIdXyzD50,
    agmpMediaPrimaryIdCustom,
    agmpMediaPrimaryIdLast = agmpMediaPrimaryIdCustom
} AgmpMediaPrimaryId;

typedef enum AgmpMediaTransferId
{
    // The first 0-255 values should match the H264 specification (see Table E-4
    // Transfer Characteristics in https://www.itu.int/rec/T-REC-H.264/en).
    agmpMediaTransferIdReserved0 = 0,
    agmpMediaTransferIdBt709 = 1,
    agmpMediaTransferIdUnspecified = 2,
    agmpMediaTransferIdReserved = 3,
    agmpMediaTransferIdGamma22 = 4,
    agmpMediaTransferIdGamma28 = 5,
    agmpMediaTransferIdSmpte170M = 6,
    agmpMediaTransferIdSmpte240M = 7,
    agmpMediaTransferIdLinear = 8,
    agmpMediaTransferIdLog = 9,
    agmpMediaTransferIdLogSqrt = 10,
    agmpMediaTransferIdIec6196624 = 11,
    agmpMediaTransferIdBt1361Ecg = 12,
    agmpMediaTransferIdIec6196621 = 13,
    agmpMediaTransferId10BitBt2020 = 14,
    agmpMediaTransferId12BitBt2020 = 15,
    agmpMediaTransferIdSmpteSt2084 = 16,
    agmpMediaTransferIdSmpteSt4281 = 17,
    agmpMediaTransferIdAribStdB67 = 18, // AKA hybrid-log gamma, HLG.

    agmpMediaTransferIdLastStandardValue = agmpMediaTransferIdSmpteSt4281,

    // Chrome-specific values start at 1000.
    agmpMediaTransferIdUnknown = 1000,
    agmpMediaTransferIdGamma24,

    // This is an ad-hoc transfer function that decodes SMPTE 2084 content into a
    // 0-1 range more or less suitable for viewing on a non-hdr display.
    agmpMediaTransferIdSmpteSt2084NonHdr,

    // TODO: Need to store an approximation of the gamma function(s).
    agmpMediaTransferIdCustom,
    agmpMediaTransferIdLast = agmpMediaTransferIdCustom,
} AgmpMediaTransferId;

typedef enum AgmpMediaMatrixId
{
    // The first 0-255 values should match the H264 specification (see Table E-5
    // Matrix Coefficients in https://www.itu.int/rec/T-REC-H.264/en).
    agmpMediaMatrixIdRgb = 0,
    agmpMediaMatrixIdBt709 = 1,
    agmpMediaMatrixIdUnspecified = 2,
    agmpMediaMatrixIdReserved = 3,
    agmpMediaMatrixIdFcc = 4,
    agmpMediaMatrixIdBt470Bg = 5,
    agmpMediaMatrixIdSmpte170M = 6,
    agmpMediaMatrixIdSmpte240M = 7,
    agmpMediaMatrixIdYCgCo = 8,
    agmpMediaMatrixIdBt2020NonconstantLuminance = 9,
    agmpMediaMatrixIdBt2020ConstantLuminance = 10,
    agmpMediaMatrixIdYDzDx = 11,

    agmpMediaMatrixIdLastStandardValue = agmpMediaMatrixIdYDzDx,
    agmpMediaMatrixIdInvalid = 255,
    agmpMediaMatrixIdLast = agmpMediaMatrixIdInvalid,
} AgmpMediaMatrixId;

typedef enum AgmpMediaRangeId
{
    // Range is not explicitly specified / unknown.
    agmpMediaRangeIdUnspecified = 0,

    // Limited Rec. 709 color range with RGB values ranging from 16 to 235.
    agmpMediaRangeIdLimited = 1,

    // Full RGB color range with RGB values from 0 to 255.
    agmpMediaRangeIdFull = 2,

    // Range is defined by TransferId/MatrixId.
    agmpMediaRangeIdDerived = 3,

    agmpMediaRangeIdLast = agmpMediaRangeIdDerived
} AgmpMediaRangeId;

struct _AgmpMediaMasteringMetadata
{
    // Red X chromaticity coordinate as defined by CIE 1931. In range [0, 1].
    float primary_r_chromaticity_x;

    // Red Y chromaticity coordinate as defined by CIE 1931. In range [0, 1].
    float primary_r_chromaticity_y;

    // Green X chromaticity coordinate as defined by CIE 1931. In range [0, 1].
    float primary_g_chromaticity_x;

    // Green Y chromaticity coordinate as defined by CIE 1931. In range [0, 1].
    float primary_g_chromaticity_y;

    // Blue X chromaticity coordinate as defined by CIE 1931. In range [0, 1].
    float primary_b_chromaticity_x;

    // Blue Y chromaticity coordinate as defined by CIE 1931. In range [0, 1].
    float primary_b_chromaticity_y;

    // White X chromaticity coordinate as defined by CIE 1931. In range [0, 1].
    float white_point_chromaticity_x;

    // White Y chromaticity coordinate as defined by CIE 1931. In range [0, 1].
    float white_point_chromaticity_y;

    // Maximum luminance. Shall be represented in candelas per square meter
    // (cd/m^2). In range [0, 9999.99].
    float luminance_max;

    // Minimum luminance. Shall be represented in candelas per square meter
    // (cd/m^2). In range [0, 9999.99].
    float luminance_min;
};

struct _AgmpMediaColorMetadata
{
    // Number of decoded bits per channel. A value of 0 indicates that the
    // BitsPerChannel is unspecified.
    unsigned int bits_per_channel;

    // The amount of pixels to remove in the Cr and Cb channels for every pixel
    // not removed horizontally. Example: For video with 4:2:0 chroma subsampling,
    // the |chroma_subsampling_horizontal| should be set to 1.
    unsigned int chroma_subsampling_horizontal;

    // The amount of pixels to remove in the Cr and Cb channels for every pixel
    // not removed vertically. Example: For video with 4:2:0 chroma subsampling,
    // the |chroma_subsampling_vertical| should be set to 1.
    unsigned int chroma_subsampling_vertical;

    // The amount of pixels to remove in the Cb channel for every pixel not
    // removed horizontally. This is additive with ChromaSubsamplingHorz. Example:
    // For video with 4:2:1 chroma subsampling, the
    // |chroma_subsampling_horizontal| should be set to 1 and
    // |cb_subsampling_horizontal| should be set to 1.
    unsigned int cb_subsampling_horizontal;

    // The amount of pixels to remove in the Cb channel for every pixel not
    // removed vertically. This is additive with |chroma_subsampling_vertical|.
    unsigned int cb_subsampling_vertical;

    // How chroma is subsampled horizontally. (0: Unspecified, 1: Left Collocated,
    // 2: Half).
    unsigned int chroma_siting_horizontal;

    // How chroma is subsampled vertically. (0: Unspecified, 1: Top Collocated, 2:
    // Half).
    unsigned int chroma_siting_vertical;

    // [HDR Metadata field] SMPTE 2086 mastering data.
    AgmpMediaMasteringMetadata mastering_metadata;

    // [HDR Metadata field] Maximum brightness of a single pixel (Maximum Content
    // Light Level) in candelas per square meter (cd/m^2).
    unsigned int max_cll;

    // [HDR Metadata field] Maximum brightness of a single full frame (Maximum
    // Frame-Average Light Level) in candelas per square meter (cd/m^2).
    unsigned int max_fall;

    // [Color Space field] The colour primaries of the video. For clarity, the
    // value and meanings for Primaries are adopted from Table 2 of
    // ISO/IEC 23001-8:2013/DCOR1. (0: Reserved, 1: ITU-R BT.709, 2: Unspecified,
    // 3: Reserved, 4: ITU-R BT.470M, 5: ITU-R BT.470BG, 6: SMPTE 170M,
    // 7: SMPTE 240M, 8: FILM, 9: ITU-R BT.2020, 10: SMPTE ST 428-1,
    // 22: JEDEC P22 phosphors).
    AgmpMediaPrimaryId primaries;

    // [Color Space field] The transfer characteristics of the video. For clarity,
    // the value and meanings for TransferCharacteristics 1-15 are adopted from
    // Table 3 of ISO/IEC 23001-8:2013/DCOR1. TransferCharacteristics 16-18 are
    // proposed values. (0: Reserved, 1: ITU-R BT.709, 2: Unspecified,
    // 3: Reserved, 4: Gamma 2.2 curve, 5: Gamma 2.8 curve, 6: SMPTE 170M,
    // 7: SMPTE 240M, 8: Linear, 9: Log, 10: Log Sqrt, 11: IEC 61966-2-4,
    // 12: ITU-R BT.1361 Extended Colour Gamut, 13: IEC 61966-2-1,
    // 14: ITU-R BT.2020 10 bit, 15: ITU-R BT.2020 12 bit, 16: SMPTE ST 2084,
    // 17: SMPTE ST 428-1 18: ARIB STD-B67 (HLG)).
    AgmpMediaTransferId transfer;

    // [Color Space field] The Matrix Coefficients of the video used to derive
    // luma and chroma values from red, green, and blue color primaries. For
    // clarity, the value and meanings for MatrixCoefficients are adopted from
    // Table 4 of ISO/IEC 23001-8:2013/DCOR1. (0:GBR, 1: BT709, 2: Unspecified,
    // 3: Reserved, 4: FCC, 5: BT470BG, 6: SMPTE 170M, 7: SMPTE 240M, 8: YCOCG,
    // 9: BT2020 Non-constant Luminance, 10: BT2020 Constant Luminance).
    AgmpMediaMatrixId matrix;

    // [Color Space field] Clipping of the color ranges. (0: Unspecified,
    // 1: Broadcast Range, 2: Full range (no clipping), 3: Defined by
    // MatrixCoefficients/TransferCharacteristics).
    AgmpMediaRangeId range;

    // [Color Space field] Only used if primaries == agmpMediaPrimaryIdCustom.
    //  This a row-major ordered 3 x 4 submatrix of the 4 x 4 transform matrix.
    // The 4th row is completed as (0, 0, 0, 1).
    float custom_primary_matrix[12];
};

#endif /* __AGMPLAYER_ES_VIDEO_COLOR_METADATA_H__ */
