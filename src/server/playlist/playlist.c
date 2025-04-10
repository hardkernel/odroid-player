// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#include "playlist.h"

t_playlist* create_playlist(void)
{
    t_playlist* playlist;

    playlist = (t_playlist*)malloc(sizeof(t_playlist));
    if (playlist == NULL)
    {
        LOG_FATAL("malloc error: %s", strerror(errno));
        return NULL;
    }

    memset(playlist->uri_arr, 0, sizeof(char*) * MAX_FILENUM);
    playlist->filenum = 0;
    playlist->cur_idx = 0;

    return playlist;
}

bool    append_playlist(t_playlist* playlist, char* uri)
{
    assert(playlist != NULL);
    assert(uri != NULL);

    playlist->uri_arr[playlist->filenum] = strdup(uri);
    playlist->filenum++;

    return true;
}

void    clear_playlist(t_playlist* playlist)
{
    if (playlist == NULL)
    {
        return;
    }

    for (size_t i = 0; i < playlist->filenum; i++)
    {
        free(playlist->uri_arr[i]);
    }

    free(playlist);
}

int get_playlist_cur_idx(t_playlist* playlist)
{
    assert(playlist != NULL);

    return playlist->cur_idx;
}

void    set_playlist_cur_idx(t_playlist* playlist, int index)
{
    assert(playlist != NULL);

    if (index < 0 || index >= playlist->filenum)
    {
        LOG_ERROR("invalid index: %d", index);
        return;
    }

    playlist->cur_idx = index;
}

void    increase_playlist_cur_idx(t_playlist* playlist)
{
    int cur_idx;

    assert(playlist != NULL);

    cur_idx = get_playlist_cur_idx(playlist);
    playlist->cur_idx = cur_idx + 1;
}

void    decrease_playlist_cur_idx(t_playlist* playlist)
{
    int cur_idx;

    assert(playlist != NULL);

    cur_idx = get_playlist_cur_idx(playlist);
    playlist->cur_idx = cur_idx - 1;
}

size_t  get_playlist_filenum(t_playlist* playlist)
{
    assert(playlist != NULL);

    return playlist->filenum;
}

char*   get_playlist_uri(t_playlist* playlist, int index)
{
    assert(playlist != NULL);

    if (index < 0 || index >= playlist->filenum)
    {
        LOG_ERROR("invalid index: %d", index);
        return NULL;
    }

    return playlist->uri_arr[index];
}

char*   get_playlist_current_uri(t_playlist* playlist)
{
    assert(playlist != NULL);

    return playlist->uri_arr[playlist->cur_idx];
}

