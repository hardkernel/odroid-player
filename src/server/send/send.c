// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#include "send.h"

static bool send_response_length(int client_fd, size_t len, int* timeout);
static bool send_response_message(int client_fd, void* send_buf, size_t len,
                                  int* timeout);

bool    send_broadcast_message(t_client_pool* client_pool, char* msg,
                               int msg_idx, Status status, Cmd cmd)
{
    t_client*           client_arr;
    int                 client_cnt;
    int                 ret;
    
    assert(client_pool != NULL);

    if (msg == NULL)
    {
        return true;
    }

    client_cnt = get_client_cnt(client_pool);
    if (client_cnt < 0)
    {
        return false;
    }

    client_arr = get_client_arr(client_pool);
    if (client_arr == NULL)
    {
        return false;
    }

    for (int i = 0; i < client_cnt; i++)
    {
        LOG_TRACE("sending broadcast message to %d...", client_arr[i].client_fd);

        ret = send_notify_message(client_arr[i].client_fd, msg, msg_idx,
                                  status, CMD_TYPE__BRDCAST, cmd);
        if (ret == false)
        {
            free(client_arr);
            return false;
        }
    }

    free(client_arr);

    return true;
}

bool    send_notify_message(int client_fd, char* msg, int msg_idx,
                            Status status, CmdType type, Cmd cmd)
{
    AMessage            send_msg = AMESSAGE__INIT;
    size_t              send_len;
    void*               send_buf;
    char**              data;
    size_t              conv_len;
    int                 ret;

    if (msg == NULL)
    {
        return true;
    }

    data = (char**)malloc(sizeof(char*));
    if (data == NULL)
    {
        LOG_FATAL("malloc error: %s", strerror(errno));
        return false;
    }
    else
    {
        data[0] = strdup(msg);
    }

    send_msg.n_data = 1;
    send_msg.data = data;
    send_msg.status = status;
    send_msg.type = type;
    send_msg.idx = msg_idx;
    send_msg.cmd = cmd;

    send_len = amessage__get_packed_size(&send_msg);
    send_buf = malloc(send_len);
    if (send_buf == NULL)
    {
        LOG_FATAL("malloc error: %s", strerror(errno));
        free(data[0]);
        free(data);
        return false;
    }

    amessage__pack(&send_msg, send_buf);

    LOG_TRACE("sending notify message to %d...", client_fd);

    conv_len = htonl(send_len);
    ret = send(client_fd, &conv_len, sizeof(conv_len), 0);
    if (ret < 0)
    {
        LOG_ERROR("send error: %s", strerror(errno));
        free(data[0]);
        free(data);
        free(send_buf);
        return false;
    }

    ret = send(client_fd, send_buf, send_len, 0);
    if (ret < 0)
    {
        LOG_ERROR("send error: %s", strerror(errno));
        free(data[0]);
        free(data);
        free(send_buf);
        return false;
    }

    free(data[0]);
    free(data);
    free(send_buf);
    return true;
}

/* Sending protobuf message process:
 * 1. Fill the data in the AMessage struct.
 * 2. Pack the AMessage struct into protobuf message.
 * 3. Send protobuf message length.
 * 4. Send protobuf message data.
 */
bool    send_response(t_node* node, t_player* player, Status status,
                      size_t n_data, char** data, int* timeout)
{
    AMessage    send_msg = AMESSAGE__INIT;
    size_t      send_len;
    void*       send_buf;
    int         ret;

    assert(node != NULL);
    assert(player != NULL);
    assert(data != NULL);

    send_msg.n_data = n_data;
    send_msg.data = data;
    send_msg.status = status;
    send_msg.type = node->type;
    send_msg.idx = node->msg_idx;
    send_msg.cmd = node->cmd;

    send_len = amessage__get_packed_size(&send_msg);
    send_buf = malloc(send_len);
    if (send_buf == NULL)
    {
        LOG_ERROR("malloc error: %s", strerror(errno));
        return false;
    }

    amessage__pack(&send_msg, send_buf);

    LOG_TRACE("sending response message to %d...", node->client_fd);

    ret = send_response_length(node->client_fd, send_len, timeout);
    if (ret == false)
    {
        free(send_buf);
        return false;
    }

    ret = send_response_message(node->client_fd, send_buf, send_len, timeout);
    if (ret == false)
    {
        free(send_buf);
        return false;
    }

    free(send_buf);

    return true;
}

static bool send_response_length(int client_fd, size_t len, int* timeout)
{
    size_t  conv_len;
    int     ret;

    conv_len = htonl(len);
    ret = send(client_fd, &conv_len, sizeof(conv_len), 0);
    if (ret < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            LOG_ERROR("send timeout");
            send_notify_message(client_fd, "server send timeout", -1,
                                STATUS__TIMEOUT, CMD_TYPE__NOTIFY, CMD__NONE);
            *timeout = true;
        }
        else
        {
            static char temp_buf[SEND_BUFFER_SIZE];

            LOG_ERROR("send error: %s", strerror(errno));
            sprintf(temp_buf, "send error: %s", strerror(errno));
            send_notify_message(client_fd, temp_buf, -1, STATUS__ESERVER,
                                CMD_TYPE__NOTIFY, CMD__NONE);
        }

        return false;
    }

    return true;
}

static bool send_response_message(int client_fd, void* send_buf, size_t len,
                                  int* timeout)
{
    int ret;

    ret = send(client_fd, send_buf, len, 0);
    if (ret < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            LOG_ERROR("send timeout");
            send_notify_message(client_fd, "server send timeout", -1,
                                STATUS__TIMEOUT, CMD_TYPE__NOTIFY, CMD__NONE);
            *timeout = true;
        }
        else
        {
            static char temp_buf[SEND_BUFFER_SIZE];

            LOG_ERROR("send error: %s", strerror(errno));
            sprintf(temp_buf, "send error: %s", strerror(errno));
            send_notify_message(client_fd, temp_buf, -1, STATUS__ESERVER,
                                CMD_TYPE__NOTIFY, CMD__NONE);
        }

        return false;
    }

    return true;
}

