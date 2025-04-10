// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#ifndef __RECEIVE_H__
# define __RECEIVE_H__

# include "../opclient.h"
# include "../monitor/monitor.h"
# include "../utils/logger.h"

# define MESSAGE_MAX_BYTE (1024 * 1024) /* 1MB */

typedef enum
{
    RECV_SUCCESS = 0,
    ERR_RECV_TIMEOUT = -1,
    ERR_RECV_CLOSED = -2,
    ERR_RECV_INTERNAL = -3,
    ERR_RECV_UNPACK = -4
} e_err_recv;

void*   run_receiver(void* data);

#endif /* __RECEIVE_H__ */

