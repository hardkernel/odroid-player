// SPDX-FileCopyrightText: 2021 Amlogic Corporation
// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#include "agmp_utils.h"

char*   cmd_none(Status* status, t_player* player)
{
    assert(player != NULL);

    *status = STATUS__INVALID;

    return NULL;
}

char*   cmd_play(Status* status, t_player* player)
{
    bool    paused;
    bool    no_child;

    assert(player != NULL);
    *status = STATUS__SUCCESS;

    pthread_mutex_lock(&player->play_mutex);
    paused = player->child_paused;
    no_child   = (player->child_pid == 0);
    pthread_mutex_unlock(&player->play_mutex);

    if (paused)
    {
        /* SIGSTOP → SIGCONT */
        command_player(player, PLAY_CMD_RESUME);
    }
    else if (no_child)
    {
        command_player(player, PLAY_CMD_REPLAY);
    }

    return strdup("play");
}

/* Terminate the running child. */
char*   cmd_stop(Status* status, t_player* player)
{
    assert(player != NULL);
    *status = STATUS__SUCCESS;

    command_player(player, PLAY_CMD_STOP);

    return strdup("stop");
}

/* SIGSTOP to the child. */
char*   cmd_pause(Status* status, t_player* player)
{
    assert(player != NULL);
    *status = STATUS__SUCCESS;

    command_player(player, PLAY_CMD_PAUSE);

    return strdup("pause");
}

/* Play next media file in the playlist. */
char*   cmd_next(Status* status, t_player* player)
{
    char*   filename;
    int     filenum;
    int     idx;

    assert(player != NULL);
    *status = STATUS__SUCCESS;

    idx = get_playlist_cur_idx(player->playlist);
    filenum = get_playlist_filenum(player->playlist);

    if (idx + 1 >= filenum)
    {
        if (!player->loop)
        {
            return strdup("End of the playlist");
        }

        filename = strdup(get_playlist_uri(player->playlist, 0));
    }
    else
    {
        filename = strdup(get_playlist_uri(player->playlist, idx + 1));
    }

    /* The running child gets signal to exit. */
    command_player(player, PLAY_CMD_NEXT);

    return filename;
}

/* Play previous media file in the playlist. */
char*   cmd_prev(Status* status, t_player* player)
{
    char*   filename;
    int     idx;

    assert(player != NULL);
    *status = STATUS__SUCCESS;

    idx = get_playlist_cur_idx(player->playlist);
    if (idx - 1 < 0)
    {
        return strdup("Start of the playlist");
    }

    filename = strdup(get_playlist_uri(player->playlist, idx - 1));

    /* The running child is signalled to exit. */
    command_player(player, PLAY_CMD_PREV);

    return filename;
}

/* Not implemented for this player */
char*   cmd_quit(Status* status, t_player* player)
{
    assert(player != NULL);

    *status = STATUS__INVALID;

    return strdup("not implemented");
}

/* Not implemented for this player */
char*   cmd_seek(Status* status, t_player* player)
{
    assert(player != NULL);

    *status = STATUS__INVALID;

    return strdup("not implemented");
}

/* This command should be delivered with STREAM type,
 * not the COMMAND type, so set it to invalid
 */
char*   cmd_time(Status* status, t_player* player)
{
    assert(player != NULL);

    *status = STATUS__INVALID;

    return strdup("should be used with stream type");
}

/* Returns current uri file name */
char*   cmd_uri(Status* status, t_player* player)
{
    assert(player != NULL);

    return strdup(get_playlist_current_uri(player->playlist));
}

/* Returns previous executed command */
char*   cmd_ack(Status* status, t_player* player)
{
    assert(player != NULL);

    if (player->prev_resp == NULL)
    {
        return strdup("couldn't find previous response");
    }

    return strdup(player->prev_resp->data[0]);
}

