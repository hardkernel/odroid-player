// SPDX-FileCopyrightText: 2021 Amlogic Corporation
// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

/*
 * This file includes modified code originally from Amlogic Corporation
 * (Apache License 2.0). The original function has been adapted for
 * use in this project by Phillip Choi.
 */

#include "agmp_utils.h"

t_agmp_info*    create_agmp_info(AGMP_HANDLE agmp_handle)
{
    t_agmp_info*    agmp_info;

    assert(agmp_handle != NULL);

    agmp_info = (t_agmp_info*)malloc(sizeof(t_agmp_info));
    if (agmp_info == NULL)
    {
        LOG_FATAL("malloc error: %s", strerror(errno));
        return NULL;
    }

    agmp_info->n_video = 0;
    agmp_info->n_audio = 0;
    agmp_info->n_text = 0;

    aamp_get_media_track_num(agmp_handle, &agmp_info->n_video,
                             &agmp_info->n_audio, &agmp_info->n_text);

    memset(&agmp_info->video_info, 0, sizeof(agmp_info->video_info));
    memset(&agmp_info->audio_info, 0, sizeof(agmp_info->audio_info));
    memset(&agmp_info->text_info, 0, sizeof(agmp_info->text_info));

    agmp_info->video_info_func = aamp_get_video_track_info;
    agmp_info->audio_info_func = aamp_get_audio_track_info;
    agmp_info->text_info_func = aamp_get_text_track_info;

    agmp_info->duration = agmp_get_duration(agmp_handle);

    return agmp_info;
}

void display_media_info(AGMP_HANDLE agmp_handle, t_player* player)
{
    char*           uri;
    t_agmp_info*    agmp_info;

    assert(agmp_handle != NULL);
    assert(player != NULL);

    agmp_info = create_agmp_info(agmp_handle);
    if (agmp_info == NULL)
    {
        LOG_ERROR("Couldn't get media data.");
        return;
    }

    uri = get_playlist_uri(player->playlist,
                           get_playlist_cur_idx(player->playlist));
    printf("\nUri: %s\n", uri);

    for (int i = 0; i < agmp_info->n_video; i++)
    {
        agmp_info->video_info_func(player->agmp_handle, i,
                                   &agmp_info->video_info);

        printf("Video Stream: %d, ", agmp_info->video_info.track_id);
        printf("Codec: %s, ", agmp_info->video_info.codec);
        printf("Container: %s, ", agmp_info->video_info.container);
        printf("Width: %d, ", agmp_info->video_info.width);
        printf("Height: %d, ", agmp_info->video_info.height);
        printf("Framerate: %d\n", agmp_info->video_info.framerate);
    }

    for (int i = 0; i < agmp_info->n_audio; i++)
    {
        agmp_info->audio_info_func(player->agmp_handle, i,
                                   &agmp_info->audio_info);

        printf("Audio Stream: %d, ", agmp_info->audio_info.track_id);
        printf("Codec: %s, ", agmp_info->audio_info.codec);
        printf("Container: %s, ", agmp_info->audio_info.container);
        printf("Samples: %d, ", agmp_info->audio_info.samples);
        printf("Channels: %d, ", agmp_info->audio_info.channels);
        printf("Rate: %d\n", agmp_info->audio_info.rate);
    }

    for (int i = 0; i < agmp_info->n_text; i++)
    {
        agmp_info->text_info_func(player->agmp_handle, i,
                                  &agmp_info->text_info);

        printf("Subtitle Stream: %d, ", agmp_info->text_info.track_id);
        printf("Language: %s\n", agmp_info->text_info.lang);
    }

    printf("\n\n");
    free(agmp_info);
}

static int  start_stream(AGMP_HANDLE agmp_handle)
{
    assert(agmp_handle != NULL);

    agmp_stop(agmp_handle, NULL); /* flush stream pipeline */
    agmp_prepare(agmp_handle, NULL); /* set new stream */
    agmp_play(agmp_handle, NULL); /* play stream */

    return false; /* value of G_SOURCE_REMOVE */
}

void    agmp_message_callback(AGMP_HANDLE agmp_handle, AGMP_MESSAGE_TYPE type,
                              void* data)
{
    t_player*   player = (t_player*)data;

    switch (type)
    {
        case AGMP_MESSAGE_BUFFERING:
        {
            LOG_DEBUG("AGMP_MESSAGE_BUFFERING: %d%%...\r", 
                     agmp_get_buffering_percent(agmp_handle));
            break;
        }
        case AGMP_MESSAGE_ASYNC_DONE:
        {
            LOG_DEBUG("AGMP_MESSAGE_ASYNC_DONE");
            display_media_info(agmp_handle, player);
            break;
        }
        case AGMP_MESSAGE_ABOUT_FINISH:
        {
            LOG_DEBUG("AGMP_MESSAGE_ABOUT_FINISH");
            if (!load_next(player))
            {
                LOG_DEBUG("reach the filelist end, stop.");
                set_player_running(player, false);
                break;
            }
			/* necessary for next stream playback */
            agmp_deferred_callback(agmp_handle, start_stream);
            break;
        }
        case AGMP_MESSAGE_EOS:
        {
            LOG_DEBUG("AGMP_MESSAGE_EOS");
			/* Actually, this player does not handle EOS signal for playback
			 * because the EOS signal is not emitted properly in some cases.
			 * Playback process was merged into "AGMP_MESSAGE_ABOUT_FINISH",
			 * which means playback would be handled before EOS signal emitted.
			 */
            if (!load_next(player))
            {
                LOG_DEBUG("reach the filelist end, stop.");
                set_player_running(player, false);
                break;
            }
            agmp_stop(agmp_handle, NULL); /* flush stream pipeline */
            agmp_prepare(agmp_handle, NULL); /* set new stream */
            agmp_play(agmp_handle, NULL); /* play stream */
            break;
        }
        case AGMP_MESSAGE_ERROR:
        {
            LOG_DEBUG("AGMP_MESSAGE_ERROR");
            set_player_running(player, false);
            break;
        }
        case AGMP_MESSAGE_VIDEO_UNDERFLOW:
        {
            LOG_DEBUG("AGMP_MESSAGE_VIDEO_UNDERFLOW");
            break;
        }
        case AGMP_MESSAGE_AUDIO_UNDERFLOW:
        {
            LOG_DEBUG("AGMP_MESSAGE_AUDIO_UNDERFLOW");
            break;
        }
        case AGMP_MESSAGE_FIRST_VFRAME:
        {
            LOG_DEBUG("AGMP_MESSAGE_FIRST_VFRAME");
            break;
        }
        case AGMP_MESSAGE_FIRST_AFRAME:
        {
            LOG_DEBUG("AGMP_MESSAGE_FIRST_AFRAME");
            break;
        }
        case AGMP_MESSAGE_MEDIA_INFO_CHANGED:
        {
            LOG_DEBUG("AGMP_MESSAGE_MEDIA_INFO_CHANGED");
            break;
        }
        default:
        {
            LOG_DEBUG("unhandled agmp signal: %d", type);
            break;
        }
    }
}

bool load_next(t_player* player)
{
    int     cur;
    size_t  filenum;

    if (player == NULL)
    {
        return false;
    }

    increase_playlist_cur_idx(player->playlist);

    cur = get_playlist_cur_idx(player->playlist);
    filenum = get_playlist_filenum(player->playlist);
    if (cur >= filenum)
    {
        if (player->loop == false)
        {
            decrease_playlist_cur_idx(player->playlist);
            return false;
        }
        else
        {
            set_playlist_cur_idx(player->playlist, 0);
            cur = 0;
        }
    }

    /* Unset current media and set it to next one */
    agmp_unset_uri(player->agmp_handle);
    agmp_set_uri(player->agmp_handle, get_playlist_uri(player->playlist, cur));

    send_broadcast_message(player->client_pool, 
                           get_playlist_uri(player->playlist, cur),
                           -1, STATUS__BRDINFO, CMD__URI);

    return true;
}

bool load_prev(t_player* player)
{
    int     cur;

    if (player == NULL)
    {
        return false;
    }

    decrease_playlist_cur_idx(player->playlist);

    cur = get_playlist_cur_idx(player->playlist);

    if (cur < 0)
    {
        increase_playlist_cur_idx(player->playlist);
        return false;
    }

    /* Unset current media and set it to previous one */
    agmp_unset_uri(player->agmp_handle);
    agmp_set_uri(player->agmp_handle, get_playlist_uri(player->playlist, cur));

    send_broadcast_message(player->client_pool,
                           get_playlist_uri(player->playlist, cur),
                           -1, STATUS__BRDINFO, CMD__URI);

    return true;
}

