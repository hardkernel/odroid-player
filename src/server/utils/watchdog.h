// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#ifndef __WATCHDOG_H__
# define __WATCHDOG_H__

# include <assert.h>
# include <stdlib.h>
# include "../send/send.h"
# include "../player/player.h"

# define WATCHDOG_TIMEOUT 1
# define WTDG_BUFFER_SIZE 128

typedef struct s_player t_player;

void*   run_watchdog(void* data);
void    start_watchdog_timer(t_player* player);
void    stop_watchdog_timer(t_player* player);

#endif /* __WATCHDOG_H__ */

