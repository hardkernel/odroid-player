// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#include "response.h"

static char**   execute_command(t_node* node, t_player* player, Status* status,
                                AGMP_CMD* cmd_arr, size_t* n_data);
static void     free_data(char** data, size_t len);
static void     save_prev_resp(t_node* node, t_player* player);

static void init_cmd_array(AGMP_CMD* cmd_array)
{
    memset(cmd_array, 0, sizeof(AGMP_CMD) * CMD__CMD_COUNT);

    cmd_array[CMD__NONE] = cmd_none;
    cmd_array[CMD__PLAY] = cmd_play;
    cmd_array[CMD__STOP] = cmd_stop;
    cmd_array[CMD__PAUSE] = cmd_pause;
    cmd_array[CMD__NEXT] = cmd_next;
    cmd_array[CMD__PREV] = cmd_prev;
    cmd_array[CMD__QUIT] = cmd_quit;    /* not implemented */
    cmd_array[CMD__SEEK] = cmd_seek;    /* not implemented */
    cmd_array[CMD__TIME] = cmd_time;    /* stream command */
    cmd_array[CMD__URI] = cmd_uri;
    cmd_array[CMD__ACK] = cmd_ack;
}

/* Response thread for sending response to client.
 * Response : result of the command execution or setting stream
 */
void*   run_response(void* data)
{
    t_player*   player;
    t_node*     node;
    Status      status;
    size_t      n_data;
    char**      resp_data;
    AGMP_CMD    cmd_arr[CMD__CMD_COUNT];
    int         ret, timeout, count;

    assert(data != NULL);

    player = (t_player*)data;

    init_cmd_array(cmd_arr);

    LOG_INFO("response thread is running...");

    while (get_player_running(player) == true)
    {
        node = dequeue(player->queue);
        if (node == NULL)
        {
            /* Node returns NULL when player is stopped */
            LOG_DEBUG("node is null, break loop");
            break;
        }

        LOG_TRACE("node: client_fd: %d, client_id: %d, msg_idx: %d,"
                  " type: %d, cmd: %d, n_data: %ld, data[0]: %s", 
                  node->client_fd, node->client_id, node->msg_idx,
                  node->type, node->cmd, node->n_data, node->data[0]);

        status = STATUS__SUCCESS;
        n_data = 0;
        resp_data = NULL;
        resp_data = execute_command(node, player, &status, cmd_arr, &n_data);
        if (resp_data == NULL)
        {
            LOG_ERROR("couldn't execute command: %s", node->data[0]);
            remove_node(node);
            continue;
        }

        /* Broadcast to all clients which command is executed */
        ret = send_broadcast_message(player->client_pool, node->data[0], -1,
                                     status, node->cmd);
        if (ret == false)
        {
            LOG_ERROR("failed to send broadcast message");
        }

        count = 0;
        while (count < RESEND_COUNT)
        {
            timeout = false;
            ret = send_response(node, player, status, n_data, resp_data,
                                &timeout);
            if (ret == true)
            {
                save_prev_resp(node, player);
                break;
            }

            if (timeout == true)
            {
                if (count++ < RESEND_COUNT)
                {
                    LOG_ERROR("client timeout. resending...");
                    sleep(1);
                    continue;
                }
            }

            LOG_ERROR("couldn't send response message");
            disconnect_client(node->client_fd, player);
            break;
        }

        free_data(resp_data, n_data);
        remove_node(node);
    }

    set_player_running(player, false);

    LOG_INFO("response thread finished");

    return NULL;
}

/* Command from client can be 2 types : COMMAND and STREAM
 * - COMMAND : used for controlling media playback, such as pausing or
 *             playing the next item
 * - STREAM  : used for sending data continuously over time
 *
 * Type COMMAND controlles the media and returns status with result of
 * the command executed.
 * 
 * Type STREAM creates stream and triggers stream thread to send data and 
 * returns status with socket paths that client should know for receiving data
 * from the endpoint.
 */
static char**   execute_command(t_node* node, t_player* player, Status* status,
                                AGMP_CMD* cmd_arr, size_t* n_data)
{
    char**  data;
    int     ret;

    assert(node != NULL);
    assert(player != NULL);

    data = NULL;
    if (node->type == CMD_TYPE__COMMAND)
    {
        *n_data = 1;
        data = (char**)malloc(sizeof(char*) * *n_data);
        if (data == NULL)
        {
            LOG_FATAL("malloc error: %s", strerror(errno));
            return NULL;
        }

        /* execute command and get the result */
        data[0] = cmd_arr[node->cmd](status, player);
        if (data[0] == NULL ||
            *status == STATUS__ESERVER ||
            *status == STATUS__INVALID)
        {
            LOG_ERROR("coudln't handle command");
            if (data[0] == NULL)
            {
                data[0] = strdup("couldn't handle command\n");
            }
        }
    }
    else if (node->type == CMD_TYPE__STREAM)
    {
        ret = check_stream_is_set(node->client_fd, player->client_pool);
        if (ret == false)
        {
            LOG_TRACE("set new stream to %d", node->client_fd);
            set_new_stream(node->client_fd, player); /* create new stream */
        }

        *n_data = 2;
        data = (char**)malloc(sizeof(char*) * *n_data);
        if (data == NULL)
        {
            LOG_FATAL("malloc error: %s", strerror(errno));
            return NULL;
        }

        /* Stream command returns status with 2 data:
         * - server stream socket path
         * - client stream socket path
         */
        data[0] = get_server_stream_path(node->client_fd, player->client_pool);
        if (data[0] == NULL)
        {
            LOG_ERROR("couldn't get server stream path");
            data[0] = strdup("couldn't get server stream path\n");
            *status = STATUS__ESERVER;
        }

        data[1] = get_client_stream_path(node->client_fd, player->client_pool);
        if (data[1] == NULL)
        {
            LOG_ERROR("couldn't get client stream path");
            data[1] = strdup("couldn't get client stream path\n");
            *status = STATUS__ESERVER;
        }

        if (*status != STATUS__ESERVER)
        {
            ret = trigger_stream(node->client_fd, player, node->cmd);
            if (ret == false)
            {
                LOG_ERROR("couldn't trigger stream thread");
                *status = STATUS__ESERVER;
            }
        }
    }
    else
    {
        LOG_ERROR("unknown type");
        *status = STATUS__ESERVER;
    }

    return data;
}

static void free_data(char** data, size_t len)
{
    if (data == NULL)
    {
        return;
    }

    for (size_t i = 0; i < len; i++)
    {
        free(data[i]);
    }

    free(data);
}

/* Used for ACK command */
static void save_prev_resp(t_node* node, t_player* player)
{
    t_node* prev_resp;

    assert(player != NULL);

    if (node == NULL)
    {
        return;
    }

    if (player->prev_resp != NULL)
    {
        remove_node(player->prev_resp);
        player->prev_resp = NULL;
    }

    prev_resp = create_node(node->client_id, node->client_fd, node->msg_idx,
                            node->status, node->type, node->cmd,
                            node->n_data, node->data);
    if (prev_resp == NULL)
    {
        return;
    }

    player->prev_resp = prev_resp;
}

