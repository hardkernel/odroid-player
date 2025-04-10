// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#ifndef __SERVER_H__
# define __SERVER_H__

# include "../client/client.h"
# include "../receive/receive.h"
# include "../player/player.h"
# include "../utils/logger.h"
# include "../utils/queue.h"

# define SERVER_SOCKET_PATH "/tmp/agm_server.sock"

typedef struct  s_player t_player;

void*   run_server(void* data);
void    disconnect_client(int client_fd, t_player* player);

#endif /* __SERVER_H__ */
