// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#ifndef __PLAYER_H__
# define __PLAYER_H__

# include "../agmp/agmplayer.h"
# include "../agmp/agmp_utils.h"
# include "../client/client.h"
# include "../playlist/playlist.h"
# include "../response/response.h"
# include "../server/server.h"
# include "../stream/stream.h"
# include "../utils/queue.h"
# include "../utils/logger.h"
# include "../utils/watchdog.h"

# define BUFFER_SIZE 1024

typedef void (*AGMP_CALLBACK)(AGMP_HANDLE, AGMP_MESSAGE_TYPE, void*);

typedef struct s_client_pool    t_client_pool;
typedef struct s_playlist       t_playlist;
typedef struct s_queue          t_queue;

typedef struct s_player
{
    t_playlist*     playlist;
    t_client_pool*  client_pool;
    t_queue*        queue;
    AGMP_HANDLE     agmp_handle;
    pthread_t       serv_thread_id;
    pthread_t       resp_thread_id;
    pthread_t       strm_thread_id;
    pthread_t       wtdg_thread_id;
    bool            running;
    bool            loop;
    fd_set          serv_set;
    fd_set          strm_set;
    int             serv_maxfd;
    int             strm_maxfd;
    t_node*         prev_resp;
    pthread_mutex_t player_mutex;
} t_player;

t_player*   create_player(bool is_loop, int log_level);
bool        prepare_player(t_player* player, char** filelist, AGMP_CALLBACK cb);
bool        start_player(t_player* player);
void        clear_player(t_player* player);
void        player_wait(t_player* player);
bool        get_player_running(t_player* player);
void        set_player_running(t_player* player, bool flag);

#endif /* __PLAYER_H__ */

