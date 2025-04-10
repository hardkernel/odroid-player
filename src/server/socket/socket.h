// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#ifndef __SOCKET_H__
# define __SOCKET_H__

# include <errno.h>
# include <fcntl.h>
# include <stdlib.h>
# include <stdio.h>
# include <sys/socket.h>
# include <string.h>
# include <sys/stat.h>
# include <sys/un.h>

# include "../utils/logger.h"
# include "../client/client.h"

# define NAME_BUFFER_SIZE 64

int         create_socket_uds(int type, char* path);
char*       create_socket_name(char const* name);

#endif /* __SOCKET_H__ */

