// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#ifndef __CLIENT_H__
# define __CLIENT_H__

# include <errno.h>
# include <fcntl.h>
# include <stdbool.h>
# include <sys/select.h>
# include <sys/socket.h>
# include <unistd.h>

# include "../socket/socket.h"
# include "../player/player.h"

# define MAX_CLIENTS 8
# define CLIENT_TIMEOUT 3

typedef struct s_player t_player;

typedef enum
{
    FIND_CLIENT_FD,
    FIND_STREAM_FD
} e_find_type;

typedef struct s_client
{
    int     client_id;
    int     client_fd;
    int     stream_fd;
    char*   server_stream_path;
    char*   client_stream_path;
    char*   (*stream_func)(t_player*);
} t_client;

typedef struct s_client_pool
{
    t_client        client_arr[MAX_CLIENTS];
    int             client_cnt;
    pthread_mutex_t client_mutex;
} t_client_pool;

t_client_pool*  create_client_pool(void);
void            clear_client_pool(t_client_pool* client_pool);
t_client*       get_client_arr(t_client_pool* client_pool);
int             get_client_cnt(t_client_pool* client_pool);
bool            add_client(int client_id, int client_fd,
                           t_client_pool* client_pool);
void            remove_client(int client_fd, t_client_pool* client_pool);
void            remove_client_stream(int stream_fd, t_client_pool* client_pool);
bool            create_client_stream(int client_fd, t_client_pool* client_pool);
bool            set_client_stream_func(int client_fd, char* (*func)(t_player*),
                                       t_client_pool* client_pool);
int             get_client_stream_fd(int client_fd, t_client_pool* client_pool);
char*           (*get_client_stream_func(int client_fd, 
                                         t_client_pool* client_pool))
                                         (t_player*);
char*           get_server_stream_path(int client_fd,
                                       t_client_pool* client_pool);
char*           get_client_stream_path(int client_fd,
                                       t_client_pool* client_pool);
bool            check_client_alive(int stream_fd, t_client_pool* client_pool);
bool            check_stream_alive(int stream_fd, t_client_pool* client_pool);
bool            check_stream_is_set(int client_fd, t_client_pool* client_pool);
int             find_client(int index, t_client_pool* client_pool);
int             find_client_id(int index, t_client_pool* client_pool);
int             find_client_index(int fd, t_client_pool* client_pool,
                                  e_find_type find);
int             find_stream(int index, t_client_pool* client_pool);

#endif /* __CLIENT_H__ */
