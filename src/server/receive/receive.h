// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#ifndef __RECEIVE_H__
# define __RECEIVE_H__

# include <errno.h>
# include <stdbool.h>
# include <stddef.h>
# include <stdint.h>
# include <stdlib.h>
# include <string.h>
# include <netinet/in.h>

# include "../send/send.h"
# include "../utils/logger.h"
# include "../../message/message.pb-c.h"

# define MESSAGE_MAX_BYTE (1024 * 1024) /* 1MB */
# define RECV_BUFFER_SIZE 128

AMessage*   receive_message(int fd, int* timeout);
void        free_message(AMessage* msg);

#endif /* __RECEIVE_H__ */

