// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#include "utils.h"

/* Example for using the UDS UDP stream */
bool    my_print_stream(OP_CLIENT* client, char* server_stream_path,
                         char* client_stream_path)
{
    int                 stream_fd, ret;
    struct sockaddr_un  server_stream_addr, client_stream_addr;
    struct timeval      t;
    char                buffer[BUFFER_SIZE];
    char                c;
    unsigned int        len;

    assert(client != NULL);
    assert(server_stream_path != NULL);
    assert(client_stream_path != NULL);

    stream_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (stream_fd < 0)
    {
        perror("socket");
        return false;
    }

    t.tv_sec = CLIENT_TIMEOUT;
    t.tv_usec = 0;
    setsockopt(stream_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&t, sizeof(t));
    setsockopt(stream_fd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&t, sizeof(t));

    memset(&server_stream_addr, 0, sizeof(server_stream_addr));
    server_stream_addr.sun_family = AF_UNIX;
    snprintf(server_stream_addr.sun_path, sizeof(server_stream_addr.sun_path),
             "%s", server_stream_path);

    memset(&client_stream_addr, 0, sizeof(client_stream_addr));
    client_stream_addr.sun_family = AF_UNIX;
    snprintf(client_stream_addr.sun_path, sizeof(client_stream_addr.sun_path),
             "%s", client_stream_path);

    ret = bind(stream_fd, (struct sockaddr*)&client_stream_addr,
               sizeof(client_stream_addr));
    if (ret < 0)
    {
        perror("bind");
        close(stream_fd);
        return false;
    }

    ret = chmod(client_stream_path, 0666);
    if (ret < 0)
    {
        /* Unlink client_stream_path will be managed by server */
        perror("chmod");
        close(stream_fd);
        return false;
    }

    /* Assume this stream function is worked with termios */
    c = 0;
    while (get_monitor_play(client->monitor) == true && c != 't')
    {
        memset(buffer, 0, sizeof(buffer));

        len = sizeof(server_stream_addr);
        ret = recvfrom(stream_fd, buffer, sizeof(buffer), 0,
                       (struct sockaddr*)&server_stream_addr, &len);
        if (ret < 0)
        {
            break;
        }
        printf("%s\r", buffer);

        ret = read(STDIN_FILENO, &c, 1);
        if (ret < 0)
        {
            continue;
        }
    }

    close(stream_fd);

    return true;
}

/* Example for using async response */
void*   my_async_receiver(void* data)
{
    OP_CLIENT*  client;
    OP_STATUS*  status;

    assert(data != NULL);

    client = (OP_CLIENT*)data;

    while (get_monitor_play(client->monitor) == true)
    {
        status = get_message_response(client);
        if (status == NULL)
        {
            LOG_ERROR("couldn't get async message\n");
            break;
        }

        if (status->type == CMD_TYPE__NOTIFY)
        {
            switch (status->status)
            {
                case STATUS__ESERVER:
                    printf("[NOTIFY]: server error: %s\n", status->data[0]);
                    // set_monitor(client, false, status->status);
                    break;
                case STATUS__TIMEOUT:
                    printf("[NOTIFY]: client request timeout\n");
                    break;
                case STATUS__RECONNT:
                    printf("[NOTIFY]: server reconnected\n");
                default:
                    break;
            }
        }
        else if (status->type == CMD_TYPE__BRDCAST)
        {
            printf("[BROADCAST] : %s\n", status->data[0]);
        }

        opc_free_status(status);
    }

    LOG_DEBUG("my_async_receiver thread finished\n");

    return NULL;
}

/* Example for using async response */
void    set_my_async_receiver(OP_CLIENT* client)
{
    int         ret;
    pthread_t   async_thread_id;

    assert(client != NULL);

    ret = pthread_create(&async_thread_id, NULL, my_async_receiver, client);
    if (ret != 0)
    {
        perror("pthread_create");
        return;
    }
    pthread_detach(async_thread_id);
}

