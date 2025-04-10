// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#ifndef __FD_SET_H__
# define __FD_SET_H__

# include <signal.h>
# include <sys/select.h>

# include "../utils/logger.h"
# include "../player/player.h"

typedef struct s_player t_player;

typedef enum {
    READ_WAIT,
    WRITE_WAIT
} e_wait;

typedef enum
{
    UPDATE_CLIENT_ADD,
    UPDATE_CLIENT_REMOVE,
} e_update;

int         wait_fd(t_player* player, e_wait type, fd_set* set, sigset_t* mask);
void        set_fd(int fd, fd_set* set);
void        unset_fd(int fd, fd_set* set);
int         fd_ready(int fd, fd_set* set);
void        init_set(fd_set* set);
void        update_fd_set(int fd, int* maxfd, fd_set* set, e_update flag);

#endif /* __FD_SET_H__ */

