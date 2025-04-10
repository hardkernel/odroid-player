// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#ifndef __OPC_UTILS_H__
# define __OPC_UTILS_H__

# include "./opclient.h"

# define EXECUTE_TIMEOUT 5

typedef struct _ODROID_PLAYER_CLIENT OP_CLIENT;
typedef struct _ODROID_PLAYER_STATUS OP_STATUS;

OP_STATUS*  opc_set_timeout(OP_CLIENT* client,
                            OP_STATUS* (*fn)(OP_CLIENT*));
int         opc_thread_timeout(OP_CLIENT* client, pthread_t tid,
                               int (*join)(pthread_t, void**));

#endif /* __OPC_UTILS_H__ */

