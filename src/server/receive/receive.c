// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#include "receive.h"

static bool     receive_message_length(int fd, size_t* len, int* timeout);
static bool     receive_message_data(int fd, uint8_t* buf, size_t len,
                                     int* timeout);
static uint8_t* create_message_buffer(size_t len);

/* Receiving protobuf message from client goes like this:
 * 1. Receive protobuf message length first.
 * 2. Receive protobuf message data based on the length.
 * 3. Unpack protobuf message to data and use it in the player
 *    - Data will be enqueued and used it for response thread
 */
AMessage*   receive_message(int fd, int* timeout)
{
    uint8_t*    recv_buf;
    AMessage*   recv_msg;
    size_t      recv_len;
    size_t      conv_len;
    int         ret;

    ret = receive_message_length(fd, &recv_len, timeout);
    if (ret == false)
    {
        LOG_ERROR("couldn't receive message length");
        return NULL;
    }

    conv_len = ntohl(recv_len);
    if (conv_len > MESSAGE_MAX_BYTE)
    {
        LOG_ERROR("invalid message length");
        send_notify_message(fd, "invalid message length", -1, STATUS__ESERVER,
                            CMD_TYPE__NOTIFY, CMD__NONE);
        return NULL;
    }

    recv_buf = create_message_buffer(conv_len);
    if (recv_buf == NULL)
    {
        return NULL;
    }

    ret = receive_message_data(fd, recv_buf, conv_len, timeout);
    if (ret == false)
    {
        free(recv_buf);
        LOG_ERROR("couldn't receive message data");
        return NULL;
    }

    recv_msg = amessage__unpack(NULL, conv_len, recv_buf);
    if (recv_msg == NULL)
    {
        LOG_ERROR("couldn't unpack recv_msg");
        free(recv_buf);
        send_notify_message(fd, "couldn't unpack message", -1, STATUS__ESERVER,
                            CMD_TYPE__NOTIFY, CMD__NONE);
        return NULL;
    }

    free(recv_buf);

    return recv_msg;
}

void    free_message(AMessage* msg)
{
    assert(msg != NULL);

    amessage__free_unpacked(msg, NULL);
}

static bool receive_message_length(int fd, size_t* len, int* timeout)
{
    int ret;

    ret = recv(fd, len, sizeof(*len), 0);
    if (ret < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            LOG_ERROR("recv timeout");
            send_notify_message(fd, "server recv timeout", -1, STATUS__TIMEOUT,
                                CMD_TYPE__NOTIFY, CMD__NONE);
            *timeout = true;
        }
        else
        {
            static char temp_buf[RECV_BUFFER_SIZE];

            LOG_ERROR("recv error: %s", strerror(errno));
            sprintf(temp_buf, "recv error: %s", strerror(errno));
            send_notify_message(fd, temp_buf, -1, STATUS__ESERVER,
                                CMD_TYPE__NOTIFY, CMD__NONE);
        }

        return false;
    }
    else if (ret == 0)
    {
        LOG_WARN("recv client closed");
        return false;
    }

    return true;
}

static bool receive_message_data(int fd, uint8_t* buf, size_t len, int* timeout)
{
    int ret;

    ret = recv(fd, buf, len, 0);
    if (ret < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            LOG_ERROR("recv timeout");
            send_notify_message(fd, "server recv timeout", -1, STATUS__TIMEOUT,
                                CMD_TYPE__NOTIFY, CMD__NONE);
            *timeout = true;
        }
        else
        {
            static char temp_buf[RECV_BUFFER_SIZE];

            LOG_ERROR("recv error: %s", strerror(errno));
            sprintf(temp_buf, "recv error: %s", strerror(errno));
            send_notify_message(fd, temp_buf, -1, STATUS__ESERVER,
                                CMD_TYPE__NOTIFY, CMD__NONE);
        }

        return false;
    }
    else if (ret == 0)
    {
        LOG_WARN("recv client closed");
        return false;
    }

    return true;
}

static uint8_t* create_message_buffer(size_t len)
{
    uint8_t*   msg_buf;

    msg_buf = (uint8_t*)malloc(len * sizeof(uint8_t));
    if (msg_buf == NULL)
    {
        LOG_FATAL("malloc error: %s", strerror(errno));
        return NULL;
    }

    memset(msg_buf, 0, len * sizeof(uint8_t));

    return msg_buf;
}

