// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#ifndef __LIB_LOADER_H__
# define __LIB_LOADER_H__

# include <dlfcn.h>
# include <stdio.h>
# include <stdlib.h>

# define SYSTEM_LIB_PATH "/usr/lib/libopclient.so"
# define OPC_LIB_PATH "../../.libs/libopclient.so"

typedef struct _ODROID_PLAYER_CLIENT OP_CLIENT;
typedef struct _ODROID_PLAYER_STATUS OP_STATUS;

typedef struct s_opc_handler
{
    void*       dl_handle;
    OP_CLIENT*  opc_client;
    OP_STATUS*  opc_status;
    OP_CLIENT*  (*opc_init)(void);
    int         (*opc_connect)(OP_CLIENT*);
    int         (*opc_prepare)(OP_CLIENT*);
    void        (*opc_destroy)(OP_CLIENT*);
    OP_STATUS*  (*opc_send_play)(OP_CLIENT*);
    OP_STATUS*  (*opc_send_stop)(OP_CLIENT*);
    OP_STATUS*  (*opc_send_pause)(OP_CLIENT*);
    OP_STATUS*  (*opc_send_next)(OP_CLIENT*);
    OP_STATUS*  (*opc_send_prev)(OP_CLIENT*);
    OP_STATUS*  (*opc_send_ack)(OP_CLIENT*);
    OP_STATUS*  (*opc_recv_time)(OP_CLIENT*);
    OP_STATUS*  (*opc_recv_amsg)(OP_CLIENT*);
    OP_STATUS*  (*opc_recv_uri)(OP_CLIENT*);
    OP_STATUS*  (*opc_create_status)(void);
    void        (*opc_free_status)(OP_STATUS*);
    int         (*opc_status_get_status)(OP_STATUS*);
    int         (*opc_status_get_type)(OP_STATUS*);
    int         (*opc_status_get_cmd)(OP_STATUS*);
    size_t      (*opc_status_get_msg_idx)(OP_STATUS*);
    size_t      (*opc_status_get_n_data)(OP_STATUS*);
    char**      (*opc_status_get_data)(OP_STATUS*);
    char**      (*opc_set_log_level)(int log_level);
} t_opc_handler;

int load_opc_library(t_opc_handler* opc_handler);
int set_opc_client(t_opc_handler* opc_handler, int log_level);

#endif /* __LIB_LOADER_H__ */

