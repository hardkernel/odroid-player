// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#ifndef __MONITOR_H__
# define __MONITOR_H__

# include "../opclient.h"
# include "../../../message/message.pb-c.h"

typedef struct _ODROID_PLAYER_CLIENT OP_CLIENT;

typedef struct s_monitor
{
    bool            play;
    Status          status;
    pthread_mutex_t monitor_mutex;
} t_monitor;

t_monitor*  create_monitor(void);
void        remove_monitor(t_monitor* monitor);
void        set_monitor(OP_CLIENT* client, bool play, Status status);
bool        get_monitor_play(t_monitor* client);
char**      get_monitor_msg(t_monitor* client);
Status      get_monitor_status(t_monitor* client);

#endif /* __MONITOR_H__ */

