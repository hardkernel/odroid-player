// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#include "socket.h"

int create_socket_uds(int type, char* path)
{
    int                 socket_fd;
    int                 ret;
    struct sockaddr_un  socket_addr;

    socket_fd = socket(AF_UNIX, type, 0);
    if (socket_fd < 0)
    {
        LOG_ERROR("couldn't create socket: %s", strerror(errno));
        return 0;
    }

    memset(&socket_addr, 0, sizeof(socket_addr));
    socket_addr.sun_family = AF_UNIX;
    snprintf(socket_addr.sun_path, sizeof(socket_addr.sun_path), "%s", path);

    unlink(path);

    ret = bind(socket_fd, (struct sockaddr*)&socket_addr, sizeof(socket_addr));
    if (ret < 0)
    {
        LOG_ERROR("couldn't bind socket: %s", strerror(errno));
        return 0;
    }

    ret = chmod(path, 0666);
    if (ret < 0)
    {
        LOG_ERROR("couldn't chmod %s: %s", path, strerror(errno));
        unlink(path);
        close(socket_fd);
        return 0;
    }

    if (type == SOCK_STREAM)
    {
        ret = listen(socket_fd, MAX_CLIENTS);
        if (ret < 0)
        {
            LOG_ERROR("couldn't listen socket: %s", strerror(errno));
            unlink(path);
            close(socket_fd);
            return 0;
        }
    }

    return socket_fd;
}

char*   create_socket_name(char const* name)
{
    char    buffer[NAME_BUFFER_SIZE];
    char*   socket_name;
    int     ret;
    size_t  cnt;

    cnt = 0;
    do
    {
        ret = snprintf(buffer, sizeof(buffer), "/tmp/%s%ld.sock", name, cnt);
        if (ret < 0)
        {
            LOG_ERROR("couldn't create socket name: %s", strerror(errno));
            return NULL;
        }

        cnt++;
    } while (access(buffer, F_OK) == 0);

    socket_name = strdup(buffer);

    return socket_name;
}

