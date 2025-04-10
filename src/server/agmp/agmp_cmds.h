// SPDX-FileCopyrightText: 2021 Amlogic Corporation
// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#ifndef __AGMP_CMDS_H__
# define __AGMP_CMDS_H__

# include "../player/player.h"
# include "../utils/watchdog.h"

#define AGMP_BUFFER_SIZE 512

typedef struct s_player t_player;

char*   cmd_none(Status* status, t_player* player);
char*   cmd_play(Status* status, t_player* player);
char*   cmd_stop(Status* status, t_player* player);
char*   cmd_pause(Status* status, t_player* player);
char*   cmd_next(Status* status, t_player* player);
char*   cmd_prev(Status* status, t_player* player);
char*   cmd_quit(Status* status, t_player* player);
char*   cmd_seek(Status* status, t_player* player);
char*   cmd_time(Status* status, t_player* player);
char*   cmd_uri(Status* status, t_player* player);
char*   cmd_ack(Status* status, t_player* player);

#endif /* __AGMP_CMDS_H__ */
