// SPDX-FileCopyrightText: 2021 Amlogic Corporation
// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#ifndef __AGMP_UTILS_H__
# define __AGMP_UTILS_H__

# include "../../message/message.pb-c.h"
# include "../player/player.h"
# include "../playlist/playlist.h"
# include "agmplayer.h"

# define AGMP_TIMEOUT 3

typedef void (*AGMP_CALLBACK)(AGMP_HANDLE, AGMP_MESSAGE_TYPE, void*);

typedef struct s_player t_player;

typedef struct s_agmp_info
{
    int         n_video;
    int         n_audio;
    int         n_text;
    VideoInfo   video_info;
    AudioInfo   audio_info;
    TextInfo    text_info;
    int         (*video_info_func)(AGMP_HANDLE, int, VideoInfo*);
    int         (*audio_info_func)(AGMP_HANDLE, int, AudioInfo*);
    int         (*text_info_func)(AGMP_HANDLE, int, TextInfo*);
    long long   duration;
} t_agmp_info;

t_agmp_info*    create_agmp_info(AGMP_HANDLE agmp_handle);
void            display_media_info(AGMP_HANDLE agmp_handle, t_player* player);
void            agmp_message_callback(AGMP_HANDLE agmp_handle,
                                      AGMP_MESSAGE_TYPE type, void* data);
bool            load_next(t_player* player);
bool            load_prev(t_player* player);

#endif /* __AGMP_UTILS_H__ */
