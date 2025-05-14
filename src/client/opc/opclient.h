#ifndef __OPC_H__
# define __OPC_H__

# include <errno.h>
# include <fcntl.h>
# include <netinet/in.h>
# include <pthread.h>
# include <setjmp.h>
# include <stdbool.h>
# include <stdio.h>
# include <stdlib.h>
# include <signal.h>
# include <string.h>
# include <sys/socket.h>
# include <sys/stat.h>
# include <sys/un.h>
# include <unistd.h>

# include "./monitor/monitor.h"
# include "./receive/receive.h"
# include "./response/response.h"
# include "./send/send.h"
# include "./utils/logger.h"
# include "./utils/queue.h"
# include "./opc_utils.h"
# include "../../message/message.pb-c.h"

# define SOCKET_PATH "/tmp/op_server.sock"
# define SERVER_TIMEOUT 3
# define CLIENT_TIMEOUT 3
# define MAX_CONNECT_WAIT 30
# define BUFFER_SIZE 64

typedef struct s_monitor t_monitor;
typedef struct s_queue t_queue;

typedef struct _ODROID_PLAYER_CLIENT
{
    int         client_fd;
    size_t      msg_idx;
    t_monitor*  monitor;
    t_queue*    msg_queue;
    t_queue*    cmd_queue;
    pthread_t   recv_thread_id;
    pthread_mutex_t	client_mutex;
    sigjmp_buf  env;
} OP_CLIENT;

typedef struct _ODROID_PLAYER_STATUS
{
    Status  status;
    CmdType type;
    Cmd     cmd;
    size_t  msg_idx;
    size_t  n_data;
    char**  data;
} OP_STATUS;

OP_CLIENT*  opc_init(void);
int         opc_prepare(OP_CLIENT* client);
int         opc_connect(OP_CLIENT* client);
void        opc_destroy(OP_CLIENT* client);
OP_STATUS*  opc_send_play(OP_CLIENT* client);
OP_STATUS*  opc_send_stop(OP_CLIENT* client);
OP_STATUS*  opc_send_pause(OP_CLIENT* client);
OP_STATUS*  opc_send_next(OP_CLIENT* client);
OP_STATUS*  opc_send_prev(OP_CLIENT* client);
OP_STATUS*  opc_send_ack(OP_CLIENT* client);
OP_STATUS*  opc_recv_time(OP_CLIENT* client);
OP_STATUS*  opc_recv_amsg(OP_CLIENT* client);
OP_STATUS*  opc_recv_uri(OP_CLIENT* client);
OP_STATUS*  opc_create_status(void);
void        opc_free_status(OP_STATUS* status);
int         opc_status_get_status(OP_STATUS* status);
int         opc_status_get_type(OP_STATUS* status);
int         opc_status_get_cmd(OP_STATUS* status);
size_t      opc_status_get_msg_idx(OP_STATUS* status);
size_t      opc_status_get_n_data(OP_STATUS* status);
char**      opc_status_get_data(OP_STATUS* status);
void        opc_set_log_level(e_log_level log_level);

#endif /* __OPC_H__ */

