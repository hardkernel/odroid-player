// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#ifndef __UTILS_H__
# define __UTILS_H__

# include "opc/opclient.h"

bool    my_print_stream(OP_CLIENT* client, char* server_stream_path,
                        char* client_stream_path);
void*   my_async_receiver(void* data);
void    set_my_async_receiver(OP_CLIENT* client);

#endif /* __UTILS_H__ */
