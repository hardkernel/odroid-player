// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#ifndef __RESPONSE_H__
# define __RESPONSE_H__

# include "../opclient.h"
# include "../utils/logger.h"
# include "../../../message/message.pb-c.h"

typedef struct _ODROID_PLAYER_STATUS OP_STATUS;
typedef struct _ODROID_PLAYER_CLIENT OP_CLIENT;

OP_STATUS*  get_command_response(OP_CLIENT* client);
OP_STATUS*  get_message_response(OP_CLIENT* client);

#endif /* __RESPONSE_H__ */

