// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#include "monitor.h"

t_monitor*  create_monitor(void)
{
    t_monitor*   monitor;

    monitor = (t_monitor*)malloc(sizeof(t_monitor));
    if (monitor == NULL)
    {
        LOG_FATAL("malloc error: %s", strerror(errno));
        return NULL;
    }

    monitor->play = true;
    monitor->status = STATUS__SUCCESS;
    pthread_mutex_init(&monitor->monitor_mutex, NULL);

    return monitor;
}

void    remove_monitor(t_monitor* monitor)
{
    if (monitor == NULL)
    {
        return;
    }

    pthread_mutex_destroy(&monitor->monitor_mutex);
    free(monitor);
}

void    set_monitor(OP_CLIENT* client, bool play, Status status)
{
    if (client == NULL || client->monitor == NULL)
    {
        return;
    }

    pthread_mutex_lock(&client->monitor->monitor_mutex);
    client->monitor->play = play;
    client->monitor->status = status;
    pthread_mutex_unlock(&client->monitor->monitor_mutex);

    /* Send signal to waiting queue */
    if (client->msg_queue)
    {
        pthread_mutex_lock(&client->msg_queue->queue_mutex);
        client->msg_queue->running = play;
        pthread_cond_signal(&client->msg_queue->queue_cond);
        pthread_mutex_unlock(&client->msg_queue->queue_mutex);
    }

    if (client->cmd_queue)
    {
        pthread_mutex_lock(&client->cmd_queue->queue_mutex);
        client->cmd_queue->running = play;
        pthread_cond_signal(&client->cmd_queue->queue_cond);
        pthread_mutex_unlock(&client->cmd_queue->queue_mutex);
    }
}

bool    get_monitor_play(t_monitor* monitor)
{
    bool    ret;

    /* Sometimes, opc_destroy removes monitor quickly than queue threads. 
     * To handle this, return false if monitor is already removed.
     */
    if (monitor == NULL)
    {
        return false;
    }

    pthread_mutex_lock(&monitor->monitor_mutex);
    ret = monitor->play;
    pthread_mutex_unlock(&monitor->monitor_mutex);

    return ret;
}

Status  get_monitor_status(t_monitor* monitor)
{
    Status  ret;

    assert(monitor != NULL);

    pthread_mutex_lock(&monitor->monitor_mutex);
    ret = monitor->status;
    pthread_mutex_unlock(&monitor->monitor_mutex);

    return ret;
}

