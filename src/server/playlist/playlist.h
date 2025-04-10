// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#ifndef __PLAYLIST_H__
# define __PLAYLIST_H__

# include <assert.h>
# include <errno.h>
# include <stdbool.h>
# include <stddef.h>
# include <stdlib.h>
# include <string.h>

# include "../utils/logger.h"

# define MAX_FILENUM 128

typedef struct s_playlist
{
    char*   uri_arr[MAX_FILENUM];
    size_t  filenum;
    int     cur_idx;
} t_playlist;

t_playlist* create_playlist(void);
bool        append_playlist(t_playlist* playlist, char* uri);
void        clear_playlist(t_playlist* playlist);
int         get_playlist_cur_idx(t_playlist* playlist);
void        set_playlist_cur_idx(t_playlist* playlist, int index);
void        increase_playlist_cur_idx(t_playlist* playlist);
void        decrease_playlist_cur_idx(t_playlist* playlist);
size_t      get_playlist_filenum(t_playlist* playlist);
char*       get_playlist_uri(t_playlist* playlist, int index);
char*       get_playlist_current_uri(t_playlist* playlist);

#endif /* __PLAYLIST_H__ */

