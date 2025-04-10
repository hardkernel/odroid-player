// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#include "fd_set.h"

// TODO : 특정 시그널만 받아서 pselect 처리하게 변경하기
int wait_fd(t_player* player, e_wait type, fd_set* set, sigset_t* mask)
{
    int ret;

    if (player == NULL)
    {
        return 0;
    }

    if (type == READ_WAIT)
    {
        *set = player->serv_set;

        ret = pselect(player->serv_maxfd + 1, set, NULL, NULL, NULL, mask);
    }
    else if (type == WRITE_WAIT)
    {
        *set = player->strm_set;

        ret = pselect(player->strm_maxfd + 1, NULL, set, NULL, NULL, mask);
    }
    else
    {
        LOG_ERROR("invalid wait type");
        return 0;
    }

    if (ret < 0 && errno != EINTR)
    {
        LOG_ERROR("pselect error: %s", strerror(errno));
        return 0;
    }
    else if (get_player_running(player) == false)
    {
        LOG_DEBUG("player stopped");
        return 0;
    }

    return ret;
}

void    set_fd(int fd, fd_set* set)
{
    FD_SET(fd, set);
}

void    unset_fd(int fd, fd_set* set)
{
    FD_CLR(fd, set);
}

int fd_ready(int fd, fd_set* set)
{
    return FD_ISSET(fd, set);
}

void    init_set(fd_set* set)
{
    FD_ZERO(set);
}

void    update_fd_set(int fd, int* maxfd, fd_set* set, e_update flag)
{
    if (fd <= 0)
    {
        LOG_ERROR("invalid fd: %d", fd);
        return;
    }

    if (flag == UPDATE_CLIENT_ADD)
    {
        set_fd(fd, set);

        if (*maxfd < fd)
        {
            *maxfd = fd;
        }
    }
    else /* UPDATE_CLIENT_REMOVE */
    {
        unset_fd(fd, set);
    } 
}

