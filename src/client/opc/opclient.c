// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#include "opclient.h"

OP_CLIENT*  g_client;

static void signal_handler(int signum)
{
    static size_t   val = 0;

    if (signum == SIGALRM)
    {
        siglongjmp(g_client->env, ++val);
    }
}

OP_CLIENT*  opc_init(void)
{
    OP_CLIENT*  client;

    client = (OP_CLIENT*)malloc(sizeof(OP_CLIENT));
    if (client == NULL)
    {
        LOG_FATAL("malloc error: %s", strerror(errno));
        return NULL;
    }

    client->client_fd = 0;
    client->msg_idx = 0;
    client->monitor = NULL;
    client->msg_queue = NULL;
    client->cmd_queue = NULL;
    client->recv_thread_id = 0;

    pthread_mutex_init(&client->client_mutex, NULL);

    /* for signal handler */
    g_client = client;

    return client;
}

int opc_prepare(OP_CLIENT* client)
{
    struct sigaction    sa;
    int                 ret;

    assert(client != NULL);

    client->monitor = create_monitor();
    if (client->monitor == NULL)
    {
        LOG_ERROR("failed to create monitor");
        return false;
    }

    client->msg_queue = create_queue();
    if (client->msg_queue == NULL)
    {
        LOG_ERROR("failed to create queue");
        remove_monitor(client->monitor);
        client->monitor = NULL;
        return false;
    }

    client->cmd_queue = create_queue();
    if (client->cmd_queue == NULL)
    {
        LOG_ERROR("failed to create queue");
        remove_monitor(client->monitor);
        remove_queue(client->msg_queue);
        client->monitor = NULL;
        client->msg_queue = NULL;
        return false;
    }

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = signal_handler;
    sigaction(SIGPIPE, &sa, NULL);
    sigaction(SIGALRM, &sa, NULL);

    ret = pthread_create(&client->recv_thread_id, NULL, run_receiver, client);
    if (ret != 0)
    {
        LOG_ERROR("failed to create thread");
        remove_monitor(client->monitor);
        remove_queue(client->msg_queue);
        remove_queue(client->cmd_queue);
        client->monitor = NULL;
        client->msg_queue = NULL;
        client->cmd_queue = NULL;
        return false;
    }

    return true;
}

int opc_connect(OP_CLIENT* client)
{
    struct sockaddr_un  addr;
    struct timeval      tv;
    int                 fd;
    int                 count;

    assert(client != NULL);

    if (client->client_fd != 0)
    {
        close(client->client_fd);
    }

    fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (client->client_fd < 0)
    {
        LOG_ERROR("socket error: %s", strerror(errno));
        return false;
    }

    tv.tv_sec = SERVER_TIMEOUT;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

    client->client_fd = fd;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path));

    count = 0;
    while (connect(client->client_fd, (struct sockaddr *)&addr, 
           sizeof(struct sockaddr_un)))
    {
        if (errno == ECONNREFUSED || errno == ENOENT)
        {
            // add broadcast?
            LOG_INFO("connecting to player...");
            sleep(1);
            count++;

            if (count >= MAX_CONNECT_WAIT)
            {
                return false;
            }
            else
            {
                continue;
            }
        }
        else
        {
            return false;
        }
    }

    LOG_INFO("server connected");
    return true;
}

void    opc_destroy(OP_CLIENT* client)
{
    assert(client != NULL);

    if (opc_thread_timeout(client, client->recv_thread_id, pthread_join) != 0)
    {
        LOG_ERROR("couldn't join run_receiver thread");
        exit(EXIT_FAILURE);
    }

    if (client->msg_queue)
    {
        LOG_DEBUG("cleaning msg_queue...");
        pthread_mutex_lock(&client->msg_queue->queue_mutex);
        client->msg_queue->running = false;
        pthread_cond_signal(&client->msg_queue->queue_cond);
        pthread_mutex_unlock(&client->msg_queue->queue_mutex);

        remove_queue(client->msg_queue);
        client->msg_queue = NULL;
    }

    if (client->cmd_queue)
    {
        LOG_DEBUG("cleaning cmd_queue...");
        pthread_mutex_lock(&client->cmd_queue->queue_mutex);
        client->cmd_queue->running = false;
        pthread_cond_signal(&client->cmd_queue->queue_cond);
        pthread_mutex_unlock(&client->cmd_queue->queue_mutex);

        remove_queue(client->cmd_queue);
        client->cmd_queue = NULL;
    }

    if (client->monitor)
    {
        LOG_DEBUG("cleaning monitor...");
        remove_monitor(client->monitor);
        client->monitor = NULL;
    }

    close(client->client_fd);
    pthread_mutex_destroy(&client->client_mutex);
    free(client);
    client = NULL;

    LOG_DEBUG("client destroyed");
}

OP_STATUS*  opc_send_play(OP_CLIENT* client)
{
    OP_STATUS*      status;
    e_err_send      err;
    int             ret;

    assert(client != NULL);

    pthread_mutex_lock(&client->client_mutex);    
    client->msg_idx++;
    ret = send_command(client, CMD_TYPE__COMMAND, CMD__PLAY, &err);
    if (ret == false)
    {
        LOG_ERROR("failed to send command: %s", "play");
    	pthread_mutex_unlock(&client->client_mutex);
        return NULL;
    }

    status = opc_set_timeout(client, get_command_response);
    pthread_mutex_unlock(&client->client_mutex);    
    return status;
}

OP_STATUS*  opc_send_stop(OP_CLIENT* client)
{
    OP_STATUS*      status;
    int             ret;
    e_err_send      err;

    assert(client != NULL);
    
    pthread_mutex_lock(&client->client_mutex);    
    client->msg_idx++;
    ret = send_command(client, CMD_TYPE__COMMAND, CMD__STOP, &err);
    if (ret == false)
    {
        LOG_ERROR("failed to send command: %s", "stop");
    	pthread_mutex_unlock(&client->client_mutex);
        return NULL;
    }

    status = opc_set_timeout(client, get_command_response);
    pthread_mutex_unlock(&client->client_mutex);    
    return status;
}

OP_STATUS*  opc_send_pause(OP_CLIENT* client)
{
    OP_STATUS*      status;
    int             ret;
    e_err_send      err;

    assert(client != NULL);
    
    pthread_mutex_lock(&client->client_mutex);    
    client->msg_idx++;
    ret = send_command(client, CMD_TYPE__COMMAND, CMD__PAUSE, &err);
    if (ret == false)
    {
        LOG_ERROR("failed to send command: %s", "pause");
    	pthread_mutex_unlock(&client->client_mutex);
        return NULL;
    }

    status = opc_set_timeout(client, get_command_response);
    pthread_mutex_unlock(&client->client_mutex);    
    return status;
}

OP_STATUS*  opc_send_next(OP_CLIENT* client)
{
    OP_STATUS*      status;
    int             ret;
    e_err_send      err;

    assert(client != NULL);

    pthread_mutex_lock(&client->client_mutex);    
    client->msg_idx++;
    ret = send_command(client, CMD_TYPE__COMMAND, CMD__NEXT, &err);
    if (ret == false)
    {
        LOG_ERROR("failed to send command: %s", "next");
    	pthread_mutex_unlock(&client->client_mutex);
        return NULL;
    }

    status = opc_set_timeout(client, get_command_response);
    pthread_mutex_unlock(&client->client_mutex);    
    return status;
}

OP_STATUS*  opc_send_prev(OP_CLIENT* client)
{
    OP_STATUS*      status;
    int             ret;
    e_err_send      err;

    assert(client != NULL);

    pthread_mutex_lock(&client->client_mutex);    
    client->msg_idx++;
    ret = send_command(client, CMD_TYPE__COMMAND, CMD__PREV, &err);
    if (ret == false)
    {
        LOG_ERROR("failed to send command: %s", "prev");
    	pthread_mutex_unlock(&client->client_mutex);
        return NULL;
    }

    status = opc_set_timeout(client, get_command_response);
    pthread_mutex_unlock(&client->client_mutex);    
    return status;
}

OP_STATUS*  opc_send_ack(OP_CLIENT* client)
{
    OP_STATUS*      status;
    int             ret;
    e_err_send      err;

    assert(client != NULL);

    pthread_mutex_lock(&client->client_mutex);    
    client->msg_idx++;
    ret = send_command(client, CMD_TYPE__COMMAND, CMD__ACK, &err);
    if (ret == false)
    {
        LOG_ERROR("failed to send command: %s", "ack");
    	pthread_mutex_unlock(&client->client_mutex);
        return NULL;
    }

    status = opc_set_timeout(client, get_command_response);
    pthread_mutex_unlock(&client->client_mutex);    
    return status;
}

OP_STATUS*  opc_recv_time(OP_CLIENT* client)
{
    OP_STATUS*      status;
    int             ret;
    e_err_send      err;

    assert(client != NULL);

    pthread_mutex_lock(&client->client_mutex);    
    client->msg_idx++;
    ret = send_command(client, CMD_TYPE__STREAM, CMD__TIME, &err);
    if (ret == false)
    {
        LOG_ERROR("failed to send command: %s", "time");
    	pthread_mutex_unlock(&client->client_mutex);
        return NULL;
    }

    status = opc_set_timeout(client, get_command_response);
    pthread_mutex_unlock(&client->client_mutex);
    return status;
}

OP_STATUS*  opc_recv_uri(OP_CLIENT* client)
{
    OP_STATUS*      status;
    int             ret;
    e_err_send      err;

    assert(client != NULL);

    pthread_mutex_lock(&client->client_mutex);    
    client->msg_idx++;
    ret = send_command(client, CMD_TYPE__COMMAND, CMD__URI, &err);
    if (ret == false)
    {
        LOG_ERROR("failed to send command: %s", "time");
    	pthread_mutex_unlock(&client->client_mutex);
        return NULL;
    }

    status = opc_set_timeout(client, get_command_response);
    pthread_mutex_unlock(&client->client_mutex);
    return status;
}

OP_STATUS*  opc_recv_amsg(OP_CLIENT* client)
{
    assert(client != NULL);

    return get_message_response(client);
}

OP_STATUS*  opc_create_status(void)
{
    OP_STATUS*  status;

    status = (OP_STATUS*)malloc(sizeof(OP_STATUS));
    if (status == NULL)
    {
        LOG_FATAL("malloc error: %s", strerror(errno));
        return NULL;
    }

    status->status = STATUS__SUCCESS;
    status->type = CMD_TYPE__COMMAND;
    status->cmd = CMD__NONE;
    status->msg_idx = 0;
    status->n_data = 0;
    status->data = NULL;

    return status;
}

void    opc_free_status(OP_STATUS* status)
{
    if (status == NULL)
    {
        return;
    }

    if (status->data)
    {
        for (size_t i = 0; i < status->n_data; i++)
        {
            free(status->data[i]);
        }
    }
    free(status->data);
    free(status);
}

int opc_status_get_status(OP_STATUS* status)
{
    assert(status != NULL);

    return status->status;
}

int opc_status_get_type(OP_STATUS* status)
{
    assert(status != NULL);

    return status->type;
}

int opc_status_get_cmd(OP_STATUS* status)
{
    assert(status != NULL);

    return status->cmd;
}

size_t  opc_status_get_msg_idx(OP_STATUS* status)
{
    assert(status != NULL);

    return status->msg_idx;
}

size_t  opc_status_get_n_data(OP_STATUS* status)
{
    assert(status != NULL);

    return status->n_data;
}

char**  opc_status_get_data(OP_STATUS* status)
{
    assert(status != NULL);

    return status->data;
}

void    opc_set_log_level(e_log_level log_level)
{
    if (log_level < TRACE || log_level > QUIET)
    {
        LOG_ERROR("invalid log level range");
        return;
    }

    set_log_level(log_level);
}

