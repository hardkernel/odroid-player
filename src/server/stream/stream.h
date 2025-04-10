// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#ifndef __STREAM_H__
# define __STREAM_H__

# include "../client/client.h"
# include "../player/player.h"
# include "../utils/logger.h"

# define SECOND 1000000000L
# define STREAM_BUFFER_SIZE 64

void*   run_stream(void* data);
bool    trigger_stream(int client_fd, t_player* player, Cmd cmd);
bool    set_new_stream(int client_fd, t_player* player);
char*   get_stream_time(t_player* player);

#endif /* __STREAM_H__ */

