// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#include "send.h"

static bool prepare_send_message(CmdType type, Cmd cmd, int idx,
                                 size_t* send_len, void** send_buf);
static void free_amessage_data(AMessage* amessage);
static bool send_message_length(OP_CLIENT* client, size_t send_len,
                                e_err_send* err);
static bool send_message_data(OP_CLIENT* client, size_t send_len,
                              void* send_buf, e_err_send* err);

bool    send_command(OP_CLIENT* client, CmdType type, Cmd command,
                     e_err_send* err)
{
    size_t  send_len;
    void*   send_buf;
    int     ret;

    if (client == NULL)
    {
        return false;
    }

    send_len = 0;
    send_buf = NULL;
    ret = prepare_send_message(type, command, client->msg_idx, &send_len, 
                               &send_buf);
    if (ret == false)
    {
        LOG_ERROR("couldn't prepare send message");
        return false;
    }

    ret = send_message_length(client, send_len, err);
    if (ret == false)
    {
        LOG_ERROR("couldn't send message length");
        free(send_buf);
        return false;
    }

    ret = send_message_data(client, send_len, send_buf, err);
    if (ret == false)
    {
        LOG_ERROR("couldn't send message data");
        free(send_buf);
        return false;
    }

    free(send_buf);
    return true;
}

static bool prepare_send_message(CmdType type, Cmd cmd, int idx,
                                 size_t* send_len, void** send_buf)
{
    AMessage    send_msg = AMESSAGE__INIT;
    char*       cmd_arr[CMD__CMD_COUNT];

    init_cmd_array(cmd_arr);

    send_msg.type = type;
    send_msg.cmd = cmd;
    send_msg.idx = idx;
    send_msg.n_data = 1;
    send_msg.data = malloc(sizeof(char*));
    if (send_msg.data == NULL)
    {
        LOG_FATAL("malloc error: %s", strerror(errno));
        return false;
    }
    send_msg.data[0] = command_to_string(cmd, cmd_arr);

    *send_len = amessage__get_packed_size(&send_msg);
    *send_buf = malloc(*send_len);
    if (*send_buf == NULL)
    {
        LOG_FATAL("malloc error: %s", strerror(errno));
        free_amessage_data(&send_msg);
        return false;
    }

    amessage__pack(&send_msg, *send_buf);
    free_amessage_data(&send_msg);

    return true;
}

static void free_amessage_data(AMessage* amessage)
{
    if (amessage == NULL)
    {
        return;
    }

    if (amessage->data)
    {
        for (int i = 0; i < amessage->n_data; i++)
        {
            free(amessage->data[i]);
        }
        free(amessage->data);
    }
}

static bool send_message_length(OP_CLIENT* client, size_t send_len,
                                e_err_send* err)
{
    size_t  conv_len;
    int     ret;

    assert(client != NULL);

    conv_len = htonl(send_len);
    ret = send(client->client_fd, &conv_len, sizeof(conv_len), 0);
    if (ret < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            LOG_ERROR("send timeout");
            *err = ERR_SEND_TIMEOUT;
        }
        else if (errno == EPIPE)
        {
            LOG_ERROR("send closed");
            *err = ERR_SEND_CLOSED;
        }
        else
        {
            LOG_ERROR("send internal error: %s", strerror(errno));
            *err = ERR_SEND_INTERNAL;
        }
        return false;
    }

    return true;
}

static bool send_message_data(OP_CLIENT* client, size_t send_len,
                              void* send_buf, e_err_send* err)
{
    int ret;

    ret = send(client->client_fd, send_buf, send_len, 0);
    if (ret < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            LOG_ERROR("send timeout");
            *err = ERR_SEND_TIMEOUT;
        }
        else if (errno == EPIPE)
        {
            LOG_ERROR("send closed");
            *err = ERR_SEND_CLOSED;
        }
        else
        {
            LOG_ERROR("send internal error: %s", strerror(errno));
            *err = ERR_SEND_INTERNAL;
        }
        return false;
    }

    return true;
}

