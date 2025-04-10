// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#ifndef __RESPONSE_H__
# define __RESPONSE_H__

# include "../agmp/agmp_cmds.h"
# include "../client/client.h"
# include "../send/send.h"
# include "../server/server.h"
# include "../player/player.h"
# include "../stream/stream.h"
# include "../utils/fd_set.h"
# include "../utils/queue.h"

# define RESEND_COUNT 3

typedef char*   (*AGMP_CMD)(Status*, t_player*);

void*   run_response(void* data);

#endif /* __RESPONSE_H__ */
