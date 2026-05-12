// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#include "stream.h"

static int      send_stream_data(int stream_fd, t_player* player);
static bool     prepare_stream(int stream_fd, struct sockaddr_un* stream_addr,
                               t_player* player);
static char*    get_stream_data(int stream_fd, t_player* player);

/* Stream thread for sending continuous data to client */
void*   run_stream(void* data)
{
    t_player*   player;
    sigset_t    empty_mask;
    fd_set      write_set;
    int         stream_fd;
    int         ret;

    assert(data != NULL);

    player = (t_player*)data;

    sigemptyset(&empty_mask);
    init_set(&write_set);

    LOG_INFO("stream thread is running...\n");

    while (get_player_running(player) == true)
    {
        ret = wait_fd(player, WRITE_WAIT, &write_set, &empty_mask);
        if (ret == 0)
        {
            break;
        }

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            stream_fd = find_stream(i, player->client_pool);
            if (stream_fd <= 0 || !fd_ready(stream_fd, &write_set))
            {
                continue;
            }

            ret = send_stream_data(stream_fd, player);
            if (ret < 0)
            {
                /* remove stream related data from client */
                update_fd_set(stream_fd, &player->strm_maxfd,
                              &player->strm_set, UPDATE_CLIENT_REMOVE);
                remove_client_stream(stream_fd, player->client_pool);
            }
        }
        usleep(1000);
    }

    set_player_running(player, false);

    LOG_INFO("stream thread finished");

    return NULL;
}

bool    trigger_stream(int client_fd, t_player* player, Cmd cmd)
{
    int ret;

    assert(client_fd > 0);
    assert(player != NULL);
    assert(player->strm_thread_id != 0);

    /* Set function to execute in the stream thread
     * TIME : sending current media position
     */
    if (cmd == CMD__TIME)
    {
        set_client_stream_func(client_fd, get_stream_time, player->client_pool);
    }

    /* Send signal to stream thread to work */
    ret = pthread_kill(player->strm_thread_id, SIGUSR1);
    if (ret > 0)
    {
        LOG_ERROR("pthread_kill error: %s\n", strerror(errno));
        return false;
    }

    return true;
}

bool    set_new_stream(int client_fd, t_player* player)
{
    int stream_fd;
    int ret;

    assert(client_fd > 0);
    assert(player != NULL);

    ret = create_client_stream(client_fd, player->client_pool);
    if (ret == false)
    {
        LOG_ERROR("couldn't set client stream");
        return false;
    }

    stream_fd = get_client_stream_fd(client_fd, player->client_pool);
    if (stream_fd <= 0)
    {
        LOG_ERROR("couldn't get stream fd");
        return false;
    }

    /* Set event for this fd to stream thread */
    update_fd_set(stream_fd, &player->strm_maxfd, &player->strm_set,
                  UPDATE_CLIENT_ADD);

    return true;
}

char*   get_stream_time(t_player* player)
{
    char        buffer[STREAM_BUFFER_SIZE];
    long long   pos;
    long        hour, minute, second, msecond;

    assert(player != NULL);

    memset(buffer, 0, sizeof(buffer));

    pos = get_player_position(player);
    if (pos < 0)
    {
        pos = 0;
    }

    hour = pos / (SECOND * 60 * 60);
    minute = (pos / (SECOND * 60)) % 60;
    second = (pos / SECOND) % 60;
    msecond = (pos % SECOND) / 100000000;

    sprintf(buffer, "%ld:%02ld:%02ld.%ld", hour, minute, second, msecond);

    return strdup(buffer);
}

static int  send_stream_data(int stream_fd, t_player* player)
{
    struct sockaddr_un  stream_addr;
    char*               stream_data;
    int                 ret;

    assert(player != NULL);

    /* Set client stream address */
    ret = prepare_stream(stream_fd, &stream_addr, player);
    if (ret == false)
    {
        LOG_ERROR("couldn't prepare stream");
        return -1;
    }

    /* Get data to send */
    stream_data = get_stream_data(stream_fd, player);
    if (stream_data == NULL)
    {
        LOG_ERROR("couldn't get stream data");
        return -1;
    }

    /* Check stream is send-able before send data */
    ret = check_stream_alive(stream_fd, player->client_pool);
    if (ret == false)
    {
        LOG_WARN("stream is not live, abort sending");
        free(stream_data);
        return -1;
    }

    ret = sendto(stream_fd, stream_data, strlen(stream_data), 0, 
                 (struct sockaddr*)&stream_addr, sizeof(stream_addr));
    if (ret < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            LOG_WARN("sendto timeout");
            free(stream_data);
            return -1;
        }
        else if (errno == ENOENT)
        {
            LOG_WARN("stream is not live, sendto failed");
            free(stream_data);
            return 0;
        }
        else
        {
            LOG_ERROR("sendto error: %s", strerror(errno));
            free(stream_data);
            return -1;
        }
    }

    free(stream_data);

    return 1;
}

static bool prepare_stream(int stream_fd, struct sockaddr_un* stream_addr,
                           t_player* player)
{
    int     client_fd;
    int     client_idx;
    char*   stream_path;

    assert(player != NULL);

    client_idx = find_client_index(stream_fd, player->client_pool, FIND_STREAM_FD);
    client_fd = find_client(client_idx, player->client_pool);
    stream_path = get_client_stream_path(client_fd, player->client_pool);
    if (stream_path == NULL)
    {
        LOG_ERROR("couldn't get client stream path");
        return false;
    }

    memset(stream_addr, 0, sizeof(*stream_addr));
    stream_addr->sun_family = AF_UNIX;
    snprintf(stream_addr->sun_path, sizeof(stream_addr->sun_path), "%s",
             stream_path);

    free(stream_path);

    return true;
}

static char*    get_stream_data(int stream_fd, t_player* player)
{
    int     client_fd;
    int     client_idx;
    char*   (*stream_func)(t_player*);

    assert(player != NULL);

    client_idx = find_client_index(stream_fd, player->client_pool, FIND_STREAM_FD);
    client_fd = find_client(client_idx, player->client_pool);

    stream_func = get_client_stream_func(client_fd, player->client_pool);
    if (stream_func == NULL)
    {
        LOG_ERROR("coulnd't get stream function");
        return NULL;
    }

    return stream_func(player);
}

