// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#ifndef __PLAYER_H__
# define __PLAYER_H__

# include <sys/types.h>
# include <sys/time.h>

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

typedef enum e_player_cmd
{
    PLAY_CMD_NONE = 0,
    PLAY_CMD_NEXT,
    PLAY_CMD_PREV,
    PLAY_CMD_STOP,
    PLAY_CMD_PAUSE,
    PLAY_CMD_RESUME,
    PLAY_CMD_REPLAY
} t_player_cmd;

typedef struct s_player
{
    t_playlist*     playlist;
    t_client_pool*  client_pool;
    t_queue*        queue;
    AGMP_HANDLE     agmp_handle;    /* unused in fork-mode, kept for ABI */
    pthread_t       serv_thread_id;
    pthread_t       resp_thread_id;
    pthread_t       strm_thread_id;
    pthread_t       wtdg_thread_id;
    pthread_t       play_thread_id;
    bool            running;
    bool            loop;
    fd_set          serv_set;
    fd_set          strm_set;
    int             serv_maxfd;
    int             strm_maxfd;
    t_node*         prev_resp;
    pthread_mutex_t player_mutex;

    pid_t           child_pid;
    bool            child_paused;
    bool            child_stopped;
    struct timeval  child_start_time;
    long long       paused_offset_us;
    struct timeval  pause_started;
    pthread_mutex_t play_mutex;
    pthread_cond_t  play_cond;
    t_player_cmd    pending_command;

    int             log_level;
} t_player;

t_player*   create_player(bool is_loop, int log_level);
bool        prepare_player(t_player* player, char** filelist, AGMP_CALLBACK cb);
bool        start_player(t_player* player);
void        clear_player(t_player* player);
void        wait_player(t_player* player);
bool        command_player(t_player* player, t_player_cmd command);
bool        get_player_running(t_player* player);
void        set_player_running(t_player* player, bool flag);
long long   get_player_position(t_player* player);

#endif /* __PLAYER_H__ */
