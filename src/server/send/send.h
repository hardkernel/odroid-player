// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#ifndef __SEND_H__
# define __SEND_H__

# include <assert.h>
# include <errno.h>
# include <fcntl.h>
# include <netinet/in.h>
# include <stdbool.h>
# include <stddef.h>
# include <stdio.h>
# include <string.h>

# include "../utils/logger.h"
# include "../utils/queue.h"
# include "../client/client.h"

# define SEND_BUFFER_SIZE 128

typedef struct s_client_pool    t_client_pool;
typedef struct s_node           t_node;

bool        send_response(t_node* node, t_player* player, Status status,
                          size_t n_data, char** data, int* timeout);
bool        send_notify_message(int client_fd, char* msg, int msg_idx,
                                Status status, CmdType type, Cmd cmd);
bool        send_broadcast_message(t_client_pool* client_pool, char* msg,
                                   int msg_idx, Status status, Cmd cmd);

#endif /* __SEND_H__ */

