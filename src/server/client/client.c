// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#include "client.h"

t_client_pool*  create_client_pool(void)
{
    t_client_pool*  client_pool;

    client_pool = (t_client_pool*)malloc(sizeof(t_client_pool));
    if (client_pool == NULL)
    {
        LOG_FATAL("malloc error: %s", strerror(errno));
        return NULL;
    }

    memset(client_pool->client_arr, 0, sizeof(t_client) * MAX_CLIENTS);
    client_pool->client_cnt = 0;
    pthread_mutex_init(&client_pool->client_mutex, NULL);

    return client_pool;
}

void    clear_client_pool(t_client_pool* client_pool)
{
    int client_fd;

    if (client_pool == NULL)
    {
        return;
    }

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        client_fd = find_client(i, client_pool);
        if (client_fd <= 0)
        {
            continue;
        }

        remove_client(client_fd, client_pool);
    }

    free(client_pool);
}

/* Beware that this function returns with allocated memory */
t_client*   get_client_arr(t_client_pool* client_pool)
{
    t_client*   client_arr;
    int         client_cnt;

    assert(client_pool != NULL);

    client_cnt = get_client_cnt(client_pool);
    if (client_cnt <= 0)
    {
        return NULL;
    }

    client_arr = (t_client*)malloc(sizeof(t_client) * client_cnt);
    if (client_arr == NULL)
    {
        LOG_FATAL("malloc error: %s", strerror(errno));
        return NULL;
    }

    pthread_mutex_lock(&client_pool->client_mutex);
    for (int i = 0; i < client_cnt; i++)
    {
        client_arr[i] = client_pool->client_arr[i];
    }
    pthread_mutex_unlock(&client_pool->client_mutex);

    return client_arr;
}

int get_client_cnt(t_client_pool* client_pool)
{
    int client_cnt;

    assert(client_pool != NULL);

    pthread_mutex_lock(&client_pool->client_mutex);
    client_cnt = client_pool->client_cnt;
    pthread_mutex_unlock(&client_pool->client_mutex);

    return client_cnt;
}

bool    add_client(int client_id, int client_fd, t_client_pool* client_pool)
{
    int             idx;
    struct timeval  t;

    assert(client_fd > 0);
    assert(client_pool != NULL);

    pthread_mutex_lock(&client_pool->client_mutex);
    idx = -1;
    while (++idx < MAX_CLIENTS)
    {
        if (client_pool->client_arr[idx].client_fd == 0)
        {
            break;
        }
    }

    if (idx >= MAX_CLIENTS)
    {
        LOG_ERROR("no space for adding new client");
        pthread_mutex_unlock(&client_pool->client_mutex);
        return false;
    }

    memset(&client_pool->client_arr[idx], 0, sizeof(t_client));

    t.tv_sec = CLIENT_TIMEOUT;
    t.tv_usec = 0;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&t, sizeof(t));
    setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&t, sizeof(t));

    client_pool->client_arr[idx].client_id = client_id;
    client_pool->client_arr[idx].client_fd = client_fd;
    client_pool->client_cnt++;
    pthread_mutex_unlock(&client_pool->client_mutex);

    return true;
}

void    remove_client(int client_fd, t_client_pool* client_pool)
{
    int idx;

    assert(client_pool != NULL);

    if (client_fd <= 0)
    {
        return;
    }

    idx = find_client_index(client_fd, client_pool, FIND_CLIENT_FD);
    if (idx < 0)
    {
        return;
    }

    remove_client_stream(get_client_stream_fd(client_fd, client_pool),
                         client_pool);

    pthread_mutex_lock(&client_pool->client_mutex);
    LOG_TRACE("removing client %d...", client_fd);

    close(client_fd);
    client_pool->client_cnt--;
    memset(&client_pool->client_arr[idx], 0, sizeof(t_client));
    pthread_mutex_unlock(&client_pool->client_mutex);
}

void    remove_client_stream(int stream_fd, t_client_pool* client_pool)
{
    int ret;
    int idx;

    assert(client_pool != NULL);

    if (stream_fd <= 0)
    {
        return;
    }

    idx = find_client_index(stream_fd, client_pool, FIND_STREAM_FD);
    if (idx < 0)
    {
        return;
    }

    ret = check_stream_alive(stream_fd, client_pool);
    if (ret == false)
    {
        return;
    }
    
    pthread_mutex_lock(&client_pool->client_mutex);
    LOG_TRACE("removing client stream: %d... idx: %d", stream_fd, idx);

    close(stream_fd);
    client_pool->client_arr[idx].stream_fd = 0;

    unlink(client_pool->client_arr[idx].server_stream_path);
    free(client_pool->client_arr[idx].server_stream_path);
    client_pool->client_arr[idx].server_stream_path = NULL;

    unlink(client_pool->client_arr[idx].client_stream_path);
    free(client_pool->client_arr[idx].client_stream_path);
    client_pool->client_arr[idx].client_stream_path = NULL;

    pthread_mutex_unlock(&client_pool->client_mutex);
}

bool    create_client_stream(int client_fd, t_client_pool* client_pool)
{
    int     idx;
    int     stream_fd;
    char*   server_socket;
    char*   client_socket;

    assert(client_pool != NULL);

    if (client_fd <= 0)
    {
        return false;
    }

    idx = find_client_index(client_fd, client_pool, FIND_CLIENT_FD);
    if (idx < 0)
    {
        return false;
    }

    pthread_mutex_lock(&client_pool->client_mutex);

    /* Remove sockets if it already allocated */
    server_socket = client_pool->client_arr[idx].server_stream_path;
    if (server_socket != NULL)
    {
        unlink(server_socket);
        free(server_socket);
    }

    client_socket = client_pool->client_arr[idx].client_stream_path;
    if (client_socket != NULL)
    {
        unlink(client_socket);
        free(client_socket);
    }

    server_socket = create_socket_name("ops_stream");
    if (server_socket == NULL)
    {
        LOG_ERROR("couldn't create socket name");
        pthread_mutex_unlock(&client_pool->client_mutex);
        return false;
    }

    client_socket = create_socket_name("opc_stream");
    if (client_socket == NULL)
    {
        LOG_ERROR("couldn't create socket name");
        free(server_socket);
        pthread_mutex_unlock(&client_pool->client_mutex);
        return false;
    }

    stream_fd = create_socket_uds(SOCK_DGRAM, server_socket);
    if (stream_fd == 0)
    {
        LOG_ERROR("couldn't create UDP socket");
        free(server_socket);
        free(client_socket);
        pthread_mutex_unlock(&client_pool->client_mutex);
        return false;
    }

    client_pool->client_arr[idx].server_stream_path = server_socket;
    client_pool->client_arr[idx].client_stream_path = client_socket;
    client_pool->client_arr[idx].stream_fd = stream_fd;
    /* Function to act with stream will be set
     * separately by 'set_client_stream_func'
     */
    client_pool->client_arr[idx].stream_func = NULL;

    pthread_mutex_unlock(&client_pool->client_mutex);

    return true;
}

bool    set_client_stream_func(int client_fd, char* (*func)(t_player*),
                               t_client_pool* client_pool)
{
    int idx;

    assert(client_fd > 0);
    assert(client_pool != NULL);

    idx = find_client_index(client_fd, client_pool, FIND_CLIENT_FD);
    if (idx < 0)
    {
        return false;
    }

    pthread_mutex_lock(&client_pool->client_mutex);
    client_pool->client_arr[idx].stream_func = func;
    pthread_mutex_unlock(&client_pool->client_mutex);

    return true;
}

int get_client_stream_fd(int client_fd, t_client_pool* client_pool)
{
    int idx;
    int stream_fd;

    assert(client_pool != NULL);

    if (client_fd <= 0)
    {
        return -1;
    }

    idx = find_client_index(client_fd, client_pool, FIND_CLIENT_FD);
    if (idx < 0)
    {
        return -1;
    }

    pthread_mutex_lock(&client_pool->client_mutex);
    stream_fd = client_pool->client_arr[idx].stream_fd;
    pthread_mutex_unlock(&client_pool->client_mutex);

    return stream_fd;
}

char*   (*get_client_stream_func(int client_fd, t_client_pool* client_pool))
                                (t_player*)
{
    char*   (*stream_func)(t_player*);
    int     idx;

    assert(client_pool != NULL);

    if (client_fd <= 0)
    {
        return NULL;
    }

    idx = find_client_index(client_fd, client_pool, FIND_CLIENT_FD);
    if (idx < 0)
    {
        return NULL;
    }

    pthread_mutex_lock(&client_pool->client_mutex);
    stream_func = client_pool->client_arr[idx].stream_func;
    pthread_mutex_unlock(&client_pool->client_mutex);

    return stream_func;
}

char*   get_server_stream_path(int client_fd, t_client_pool* client_pool)
{
    int     idx;
    char*   server_stream_path;

    assert(client_pool != NULL);

    if (client_fd <= 0)
    {
        return NULL;
    }

    idx = find_client_index(client_fd, client_pool, FIND_CLIENT_FD);
    if (idx < 0)
    {
        return NULL;
    }

    pthread_mutex_lock(&client_pool->client_mutex);
    server_stream_path = client_pool->client_arr[idx].server_stream_path;
    pthread_mutex_unlock(&client_pool->client_mutex);

    return server_stream_path == NULL ? NULL : strdup(server_stream_path);
}

char*   get_client_stream_path(int client_fd, t_client_pool* client_pool)
{
    int     idx;
    char*   client_stream_path;

    assert(client_pool != NULL);

    if (client_fd <= 0)
    {
        return NULL;
    }

    idx = find_client_index(client_fd, client_pool, FIND_CLIENT_FD);
    if (idx < 0)
    {
        return NULL;
    }

    pthread_mutex_lock(&client_pool->client_mutex);
    client_stream_path = client_pool->client_arr[idx].client_stream_path;
    pthread_mutex_unlock(&client_pool->client_mutex);

    return client_stream_path == NULL ? NULL : strdup(client_stream_path);
}

bool    check_client_alive(int stream_fd, t_client_pool* client_pool)
{
    int idx;

    assert(client_pool != NULL);

    if (stream_fd <= 0)
    {
        return false;
    }

    idx = find_client_index(stream_fd, client_pool, FIND_CLIENT_FD);
    if (idx < 0)
    {
        return false;
    }

    /* Assume that finding client using stream_fd can be considered client
     * is alive because it means stream_fd is followed with client element
     */
    return true;
}

bool    check_stream_alive(int stream_fd, t_client_pool* client_pool)
{
    int idx;

    assert(client_pool != NULL);

    if (stream_fd <= 0)
    {
        return false;
    }

    idx = find_client_index(stream_fd, client_pool, FIND_STREAM_FD);
    if (idx < 0)
    {
        return false;
    }

    pthread_mutex_lock(&client_pool->client_mutex);
    if (client_pool->client_arr[idx].server_stream_path == NULL ||
        client_pool->client_arr[idx].client_stream_path == NULL ||
        client_pool->client_arr[idx].stream_fd <= 0)
    {
        pthread_mutex_unlock(&client_pool->client_mutex);
        return false;
    }
    pthread_mutex_unlock(&client_pool->client_mutex);

    /* Assume that all stream-related elements are exists
     * can be considered client's stream is still exists and usable
     */
    return true;
}

bool    check_stream_is_set(int client_fd, t_client_pool* client_pool)
{
    int idx;

    assert(client_pool != NULL);

    if (client_fd <= 0)
    {
        return false;
    }

    /* This function is same with 'check_stream_alive', but
     * the argument is 'client_fd'
     */
    idx = find_client_index(client_fd, client_pool, FIND_CLIENT_FD);
    if (idx < 0)
    {
        return false;
    }

    pthread_mutex_lock(&client_pool->client_mutex);
    if (client_pool->client_arr[idx].server_stream_path == NULL ||
        client_pool->client_arr[idx].client_stream_path == NULL ||
        client_pool->client_arr[idx].stream_fd <= 0)
    {
        pthread_mutex_unlock(&client_pool->client_mutex);
        return false;
    }
    pthread_mutex_unlock(&client_pool->client_mutex);

    return true;
}

int find_client(int index, t_client_pool* client_pool)
{
    int client_fd;

    assert(client_pool != NULL);

    if (index < 0 || index >= MAX_CLIENTS)
    {
        return -1;
    }

    pthread_mutex_lock(&client_pool->client_mutex);
    client_fd = client_pool->client_arr[index].client_fd;
    pthread_mutex_unlock(&client_pool->client_mutex);

    return client_fd;
}

int find_client_id(int index, t_client_pool* client_pool)
{
    int client_fd;

    assert(client_pool != NULL);

    if (index < 0 || index >= MAX_CLIENTS)
    {
        return -1;
    }

    pthread_mutex_lock(&client_pool->client_mutex);
    client_fd = client_pool->client_arr[index].client_id;
    pthread_mutex_unlock(&client_pool->client_mutex);

    return client_fd;
}

/* It can find client index using client_fd and stream_fd both */
int find_client_index(int fd, t_client_pool* client_pool, e_find_type find)
{
    int idx;

    assert(client_pool != NULL);

    if (fd <= 0)
    {
        return -1;
    }

    pthread_mutex_lock(&client_pool->client_mutex);
    idx = -1;
    if (find == FIND_CLIENT_FD)
    {
        while (++idx < MAX_CLIENTS)
        {
            if (client_pool->client_arr[idx].client_fd == fd)
            {
                break;
            }
        }
    }
    else /* FIND_STREAM_FD */
    {
        while (++idx < MAX_CLIENTS)
        {
            if (client_pool->client_arr[idx].stream_fd == fd)
            {
                break;
            }
        }
    }
    pthread_mutex_unlock(&client_pool->client_mutex);

    if (idx >= MAX_CLIENTS)
    {
        return -1;
    }

    return idx;
}

int find_stream(int index, t_client_pool* client_pool)
{
    int stream_fd;

    assert(client_pool != NULL);

    if (index < 0 || index >= MAX_CLIENTS)
    {
        return -1;
    }

    pthread_mutex_lock(&client_pool->client_mutex);
    stream_fd = client_pool->client_arr[index].stream_fd;
    pthread_mutex_unlock(&client_pool->client_mutex);

    return stream_fd;
}

