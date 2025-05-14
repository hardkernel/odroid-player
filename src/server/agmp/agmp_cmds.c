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

/* Play the media stream */
char*   cmd_play(Status* status, t_player* player)
{
    AGMP_SSTATUS    player_status;
    static char     agmp_msg[AGMP_BUFFER_SIZE];
    int             ret;

    assert(player != NULL);

    *status = STATUS__SUCCESS;
    player_status = agmp_get_state(player->agmp_handle);
    if (player_status == AGMP_STATUS_STOPPED)
    {
        start_watchdog_timer(player);
        ret = agmp_prepare(player->agmp_handle, agmp_msg);
        stop_watchdog_timer(player);
        if (ret > 0)
        {
            LOG_ERROR("%s", agmp_msg);
            *status = STATUS__ESERVER;
            return strdup(agmp_msg);
        }
    }

    start_watchdog_timer(player);
    ret = agmp_play(player->agmp_handle, agmp_msg);
    stop_watchdog_timer(player);
    if (ret > 0)
    {
        LOG_ERROR("%s", agmp_msg);
        *status = STATUS__ESERVER;
    }

    return strdup(agmp_msg);
}

/* Stop the media stream */
char*   cmd_stop(Status* status, t_player* player)
{
    static char     agmp_msg[AGMP_BUFFER_SIZE];
    int             ret;

    assert(player != NULL);

    *status = STATUS__SUCCESS;

    start_watchdog_timer(player);
    ret = agmp_stop(player->agmp_handle, agmp_msg);
    stop_watchdog_timer(player);
    if (ret > 0)
    {
        LOG_ERROR("%s", agmp_msg);
        *status = STATUS__ESERVER;
    }

    return strdup(agmp_msg);
}

/* Pause the media stream */
char*   cmd_pause(Status* status, t_player* player)
{
    static char     agmp_msg[AGMP_BUFFER_SIZE];
    int             ret;

    assert(player != NULL);

    *status = STATUS__SUCCESS;

    start_watchdog_timer(player);
    ret = agmp_pause(player->agmp_handle, agmp_msg);
    stop_watchdog_timer(player);
    if (ret > 0)
    {
        LOG_ERROR("%s", agmp_msg);
        *status = STATUS__ESERVER;
    }

    return strdup(agmp_msg);
}

/* Play next media file */
char*   cmd_next(Status* status, t_player* player)
{
    static char     agmp_msg[AGMP_BUFFER_SIZE];
    char*           filename;
    int             cur_idx;
    int             filenum;
    int             ret;

    assert(player != NULL);

    *status = STATUS__SUCCESS;

    cur_idx = get_playlist_cur_idx(player->playlist);
    filenum = get_playlist_filenum(player->playlist);
    if (cur_idx + 1 >= filenum)
    {
        filename = strdup("End of the playlist");
        return filename;
    }
    
    /* Get next media file and set to the player */
    filename = strdup(get_playlist_uri(player->playlist, cur_idx + 1));
    load_next(player);

    /* Stop player to flush media stream */
    start_watchdog_timer(player);
    ret = agmp_stop(player->agmp_handle, agmp_msg);
    stop_watchdog_timer(player);
    if (ret > 0)
    {
        free(filename);
        LOG_ERROR("%s", agmp_msg);
        *status = STATUS__ESERVER;
        return strdup(agmp_msg);
    }

    /* Set next media stream */
    start_watchdog_timer(player);
    ret = agmp_prepare(player->agmp_handle, agmp_msg);
    stop_watchdog_timer(player);
    if (ret > 0)
    {
        free(filename);
        LOG_ERROR("%s", agmp_msg);
        *status = STATUS__ESERVER;
        return strdup(agmp_msg);
    }

    /* Play media stream */
    start_watchdog_timer(player);
    ret = agmp_play(player->agmp_handle, agmp_msg);
    stop_watchdog_timer(player);
    if (ret > 0)
    {
        free(filename);
        LOG_ERROR("%s", agmp_msg);
        *status = STATUS__ESERVER;
        return strdup(agmp_msg);
    }

    return filename;
}

/* Play previous media file */
char*   cmd_prev(Status* status, t_player* player)
{
    static char     agmp_msg[AGMP_BUFFER_SIZE];
    char*           filename;
    int             cur_idx;
    int             ret;

    assert(player != NULL);

    *status = STATUS__SUCCESS;

    cur_idx = get_playlist_cur_idx(player->playlist);
    if (cur_idx - 1 < 0)
    {
        filename = strdup("Start of the playlist");
        return filename;
    }
    
    /* Get previous media file and set to the player */
    filename = strdup(get_playlist_uri(player->playlist, cur_idx - 1));
    load_prev(player);

    /* Stop player to flush media stream */
    start_watchdog_timer(player);
    ret = agmp_stop(player->agmp_handle, agmp_msg);
    stop_watchdog_timer(player);
    if (ret > 0)
    {
        free(filename);
        LOG_ERROR("%s", agmp_msg);
        *status = STATUS__ESERVER;
        return strdup(agmp_msg);
    }

    /* Set previous media stream */
    start_watchdog_timer(player);
    ret = agmp_prepare(player->agmp_handle, agmp_msg);
    stop_watchdog_timer(player);
    if (ret > 0)
    {
        free(filename);
        LOG_ERROR("%s", agmp_msg);
        *status = STATUS__ESERVER;
        return strdup(agmp_msg);
    }

    /* Play media stream */
    start_watchdog_timer(player);
    ret = agmp_play(player->agmp_handle, agmp_msg);
    stop_watchdog_timer(player);
    if (ret > 0)
    {
        free(filename);
        LOG_ERROR("%s", agmp_msg);
        *status = STATUS__ESERVER;
        return strdup(agmp_msg);
    }

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

