// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#ifndef __SEND_H__
# define __SEND_H__

# include "../opclient.h"
# include "../opc_cmd.h"
# include "../utils/logger.h"

typedef struct _ODROID_PLAYER_CLIENT OP_CLIENT;

typedef enum
{
    SEND_SUCCESS = 0,
    ERR_SEND_TIMEOUT = -1,
    ERR_SEND_CLOSED = -2,
    ERR_SEND_INTERNAL = -3
} e_err_send;

bool    send_command(OP_CLIENT* client, CmdType type, Cmd command,
                     e_err_send* err);

#endif /* __SEND_H__ */

