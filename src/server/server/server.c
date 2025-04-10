// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#include "server.h"

static int  connect_new_client(int server_fd, t_player* player, int* full);

/* Server thread for managing clients and receiving message from clients */
void*   run_server(void* data)
{
    t_player*   player;
    t_node*     node;
    sigset_t    empty_mask;
    fd_set      read_set;
    int         server_fd;
    int         client_fd, client_id;
    int         ret;

    assert(data != NULL);

    player = (t_player*)data;

    server_fd = create_socket_uds(SOCK_STREAM, SERVER_SOCKET_PATH);
    if (server_fd == 0)
    {
        LOG_ERROR("couldn't start server thread");
        goto stop_server;
    }

    sigemptyset(&empty_mask);
    set_fd(server_fd, &player->serv_set);
    player->serv_maxfd = server_fd;
    init_set(&read_set);

    LOG_INFO("server threads is running...");

    while (get_player_running(player) == true)
    {
        ret = wait_fd(player, READ_WAIT, &read_set, &empty_mask);
        if (ret < 0 && errno == EINTR)
        {
            /* wait_fd catches the signal to check player should be running.
             * If wait_fd got signal, but player stop flag didn't set yet,
             * skip this condition for now.
             */
            continue;
        }
        else if (ret == 0)
        {
            break;
        }

        /* Event occured from server - connect new client */
        if (fd_ready(server_fd, &read_set))
        {
            int full;

            full = false;
            client_fd = connect_new_client(server_fd, player, &full);
            if (client_fd == 0)
            {
                if (full == true)
                {
                    send_notify_message(client_fd, "too many clients", -1,
                                        STATUS__TOOMANY, CMD_TYPE__NOTIFY,
                                        CMD__NONE);
                    close(client_fd);
                }

                LOG_ERROR("couldn't accpet new client");
                continue;
            }

            update_fd_set(client_fd, &player->serv_maxfd, &player->serv_set,
                              UPDATE_CLIENT_ADD);

            LOG_DEBUG("new client connected: %d", client_fd);
        }

        /* Event occured from clients - receive client message and enqueue */
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            AMessage*   client_msg;
            int         timeout;

            client_fd = find_client(i, player->client_pool);
            if (client_fd <= 0 || !fd_ready(client_fd, &read_set))
            {
                continue;
            }

            client_id = find_client_id(i, player->client_pool);
            if (client_id < 0)
            {
                LOG_ERROR("invalid client id");
                continue;
            }

            LOG_DEBUG("fd: %d, client: %d got event", client_fd, client_id);

            /* Receive protobuf message from client */
            timeout = false;
            client_msg = receive_message(client_fd, &timeout);
            if (client_msg == NULL)
            {
                if (timeout == true)
                {
                    continue;
                }

                LOG_ERROR("couldn't receive client message");
                send_notify_message(client_fd, "couldn't receive client message",
                                    -1, STATUS__ESERVER, CMD_TYPE__NOTIFY,
                                    CMD__NONE);
                disconnect_client(client_fd, player);
                continue;
            }

            LOG_TRACE("got message from client %d: %s, %d", client_id,
                                                            client_msg->data[0],
                                                            client_msg->type);

            /* Prepare to enqueue unpacked message data */
            node = create_node(client_id, client_fd, client_msg->idx, 
                               STATUS__SUCCESS, client_msg->type, 
                               client_msg->cmd, client_msg->n_data,
                               client_msg->data);
            if (node == NULL)
            {
                LOG_ERROR("couldn't create node");
                free_message(client_msg);
                continue;
            }

            ret = enqueue(player->queue, node);
            if (ret == false)
            {
                LOG_ERROR("couldn't enqueue");
                send_notify_message(client_fd, "couldn't enqueue", -1,
                                    STATUS__ESERVER, CMD_TYPE__NOTIFY,
                                    CMD__NONE);
                remove_node(node);
                free_message(client_msg);
                continue;
            }

            free_message(client_msg);
        }
    }

stop_server:
    if (server_fd > 0)
    {
        close(server_fd);
        unlink(SERVER_SOCKET_PATH);
    }

    set_player_running(player, false);

    LOG_INFO("server thread finished");

    return NULL;
}

static int  connect_new_client(int server_fd, t_player* player, int* full)
{
    static size_t       client_id;
    int                 client_fd;
    struct sockaddr_un  client_addr;
    unsigned int        len;
    int                 client_cnt;
    int                 ret;

    assert(player != NULL);

    len = sizeof(client_addr);
    client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &len);
    if (client_fd < 0)
    {
        LOG_ERROR("couldn't accept client");
        return 0;
    }

    client_cnt = get_client_cnt(player->client_pool);
    if (client_cnt < 0)
    {
        LOG_ERROR("couldn't get client_cnt");
        close(client_fd);
        return 0;
    }
    else if (client_cnt >= MAX_CLIENTS)
    {
        LOG_WARN("too many clients");
        *full = true;
        return 0;
    }

    ret = add_client(client_id++, client_fd, player->client_pool);
    if (ret == false)
    {
        LOG_ERROR("couldn't add new client");
        send_notify_message(client_fd, "couldn't add new client to server", -1,
                            STATUS__ESERVER, CMD_TYPE__NOTIFY, CMD__NONE);
        close(client_fd);
        return 0;
    }

    return client_fd;
}

void disconnect_client(int client_fd, t_player* player)
{
    int stream_fd;

    assert(player != NULL);
    assert(client_fd > 0);

    update_fd_set(client_fd, &player->serv_maxfd, &player->serv_set,
                  UPDATE_CLIENT_REMOVE);
    stream_fd = get_client_stream_fd(client_fd, player->client_pool);
    if (stream_fd > 0)
    {
        update_fd_set(stream_fd, &player->strm_maxfd, &player->strm_set,
                      UPDATE_CLIENT_REMOVE);
    }
    remove_client(client_fd, player->client_pool);
}

