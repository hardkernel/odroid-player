#include "receive.h"

static int          receive_message(OP_CLIENT* client, AMessage** recv_msg);
static size_t       receive_message_length(OP_CLIENT* client, 
                                           e_err_recv* err);
static uint8_t*     create_message_buffer(size_t len);
static AMessage*    receive_message_data(OP_CLIENT* client, uint8_t* buf,
                                         size_t len, e_err_recv* err);
static int          handle_message(OP_CLIENT* client, AMessage* recv_msg);
static int          notice_reconnect(OP_CLIENT* client, bool closed);
// static bool         is_socket_exist(char* path);

void*   run_receiver(void* data)
{
    OP_CLIENT*  client;
    AMessage*   recv_msg;
    int         ret;

    assert(data != NULL);

    client = (OP_CLIENT*)data;
    recv_msg = NULL;

    LOG_INFO("receive thread is running...");

    while (get_monitor_play(client->monitor) == true)
    {
        ret = receive_message(client, &recv_msg);
        if (ret < 0)
        {
            LOG_ERROR("couldn't receive message");
            break;
        }
        else if (ret == 0)
        {
            continue;
        }
        else
        {
            LOG_TRACE("received - status: %d, cmd: %d, type: %d, idx: %d", 
                      recv_msg->status, recv_msg->cmd, recv_msg->type, 
                      recv_msg->idx);
            for (int i = 0; i < recv_msg->n_data; i++)
            {
                LOG_TRACE("received - %s", recv_msg->data[i]);
            }
        }

        handle_message(client, recv_msg);
        amessage__free_unpacked(recv_msg, NULL);
    }

    set_monitor(client, false, STATUS__ESERVER);

    LOG_INFO("receive thread finished");
    return NULL;
}

static int  receive_message(OP_CLIENT* client, AMessage** recv_msg)
{
    size_t      recv_len, conv_len;
    uint8_t*    recv_buf;
    e_err_recv  err;

    assert(client != NULL);

    err = RECV_SUCCESS;

    recv_len = receive_message_length(client, &err);
    if (recv_len == 0)
    {
        if (err == ERR_RECV_TIMEOUT)
        {
            // LOG_DEBUG("receive timeout");
            return 0;
        }
        else if (err == ERR_RECV_CLOSED)
        {
            LOG_ERROR("recv closed");
            notice_reconnect(client, true);

            if (opc_connect(client) == true)
            {
                notice_reconnect(client, false);
                return 0;
            }
//          if (is_socket_exist(SOCKET_PATH) == false &&
//              agmc_connect(client) == true)
//          {
//              /* wait for reconnecting server */
//              continue;
//          }
        }
        else
        {
            LOG_ERROR("recv internal error: %s", strerror(errno));
        }

        return -1;
    }

    conv_len = ntohl(recv_len);
    if (conv_len > MESSAGE_MAX_BYTE)
    {
        LOG_ERROR("invalid message length");
        return -1;
    }

    recv_buf = create_message_buffer(conv_len);
    if (recv_buf == NULL)
    {
        LOG_ERROR("couldn't create message buffer");
        return -1;
    }

    *recv_msg = receive_message_data(client, recv_buf, conv_len, &err);
    free(recv_buf);
    if (*recv_msg == NULL)
    {
        if (err == ERR_RECV_TIMEOUT)
        {
            LOG_DEBUG("recv timeout");
            return 0;
        }
        else if (err == ERR_RECV_CLOSED)
        {
            LOG_ERROR("recv closed");
            notice_reconnect(client, true);

            if (opc_connect(client) == true)
            {
                notice_reconnect(client, false);
                return 0;
            }
//            if (is_socket_exist(SOCKET_PATH) == false &&
//                agmc_connect(client) == true)
//            {
//                /* wait for reconnecting server */
//                continue;
//            }
        }
        else if (err == ERR_RECV_UNPACK)
        {
            LOG_ERROR("couldn't unpack received message");
        }
        else
        {
            LOG_ERROR("recv internal error: %s", strerror(errno));
        }

        return -1;
    }

    return 1;
}

static size_t   receive_message_length(OP_CLIENT* client, e_err_recv* err)
{
    size_t  recv_len;
    int     ret;

    ret = recv(client->client_fd, &recv_len, sizeof(recv_len), 0);
    if (ret < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            *err = ERR_RECV_TIMEOUT;
        }
        else if (errno == ECONNRESET)
        {
            *err = ERR_RECV_CLOSED;
        }
        else
        {
            *err = ERR_RECV_INTERNAL;
        }

        return 0;
    }
    else if (ret == 0)
    {
        *err = ERR_RECV_CLOSED;

        return 0;
    }

    return recv_len;
}

static uint8_t* create_message_buffer(size_t len)
{
    uint8_t*    recv_buf;

    recv_buf = (uint8_t*)malloc(sizeof(uint8_t) * len);
    if (recv_buf == NULL)
    {
        return NULL;
    }

    memset(recv_buf, 0, sizeof(uint8_t) * len);

    return recv_buf;
}

static AMessage*    receive_message_data(OP_CLIENT* client, uint8_t* buf,
                                         size_t len, e_err_recv* err)
{
    AMessage*   recv_msg;
    int         ret;

    ret = recv(client->client_fd, buf, len, 0);
    if (ret < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            *err = ERR_RECV_TIMEOUT;
        }
        else if (errno == ECONNRESET)
        {
            *err = ERR_RECV_CLOSED;
        }
        else
        {
            *err = ERR_RECV_INTERNAL;
        }

        return NULL;
    }
    else if (ret == 0)
    {
        *err = ERR_RECV_CLOSED;

        return NULL;
    }

    recv_msg = amessage__unpack(NULL, len, buf);
    if (recv_msg == NULL)
    {
        *err = ERR_RECV_UNPACK;

        return NULL;
    }

    return recv_msg;
}

static int  handle_message(OP_CLIENT* client, AMessage* recv_msg)
{
    int ret;

    assert(recv_msg != NULL);

    if (recv_msg->type == CMD_TYPE__COMMAND ||
        recv_msg->type == CMD_TYPE__STREAM)
    {
        ret = enqueue(client->cmd_queue, create_node(client->client_fd,
                                                     recv_msg->status,
                                                     recv_msg->type,
                                                     recv_msg->cmd,
                                                     recv_msg->idx,
                                                     recv_msg->n_data,
                                                     recv_msg->data));
        if (ret == false)
        {
            LOG_ERROR("queue is full");
        }
    }
    else if (recv_msg->type == CMD_TYPE__NOTIFY ||
             recv_msg->type == CMD_TYPE__BRDCAST)
    {
        if (recv_msg->status == STATUS__TOOMANY)
        {
            /* Too many clients in the server. quit this client */
            LOG_ERROR("too many clients in the server. quitting...");
            set_monitor(client, false, recv_msg->status);
            return 0;
        }
        else if (recv_msg->status == STATUS__ESERVER)
        {
            LOG_WARN("server error: %s", recv_msg->data[0]);
        }
        else if (recv_msg->status == STATUS__TIMEOUT)
        {
            LOG_WARN("client request timeout");
        }

        ret = enqueue(client->msg_queue, create_node(client->client_fd,
                                                     recv_msg->status,
                                                     recv_msg->type,
                                                     recv_msg->cmd,
                                                     recv_msg->idx,
                                                     recv_msg->n_data,
                                                     recv_msg->data));
        if (ret == false)
        {
            LOG_ERROR("queue is full");
        }
    }
    else
    {
        LOG_ERROR("unknown message type received: %d", recv_msg->type);
    }

    return 1;
}

static int  notice_reconnect(OP_CLIENT* client, bool closed)
{
    int     ret;
    size_t  n_data;
    char**  data;

    assert(client != NULL);

    /* If server is closed */
    if (closed == true)
    {
        n_data = 1;
        data = (char**)malloc(sizeof(char*));
        if (data == NULL)
        {
            LOG_FATAL("malloc error: %s", strerror(errno));
            return 0;
        }
        data[0] = strdup("server closed");

        ret = enqueue(client->msg_queue, create_node(client->client_fd,
                                                     STATUS__ESERVER,
                                                     CMD_TYPE__NOTIFY,
                                                     CMD__NONE,
                                                     -1, n_data, data));
        free(data[0]);
        free(data);
        if (ret == false)
        {
            LOG_ERROR("queue is full");
            flush_queue(client->msg_queue);
        }

        return 1;
    }

    /* If server is reconnected */
    n_data = 0;
    data = NULL;
    ret = enqueue(client->msg_queue, create_node(client->client_fd,
                                                 STATUS__RECONNT,
                                                 CMD_TYPE__NOTIFY,
                                                 CMD__NONE,
                                                 -1, n_data, data));
    if (ret == false)
    {
        LOG_ERROR("queue is full");
        flush_queue(client->msg_queue);
    }

    return 1;
}

//static bool is_socket_exist(char* path)
//{
//    if (access(path, F_OK) == 0)
//    {
//        return true;
//    }
//
//    return false;
//}

