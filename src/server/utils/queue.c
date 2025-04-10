// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#include "queue.h"

t_node* create_node(int client_id, int client_fd, int msg_idx, Status status,
                    CmdType type, Cmd cmd, size_t n_data, char** data)
{
    t_node*    node;

    node = (t_node*)malloc(sizeof(t_node));
    if (!node)
    {
        LOG_FATAL("malloc error: %s", strerror(errno));
        return NULL;
    }

    node->client_id = client_id;
    node->client_fd = client_fd;
    node->status = status;
    node->msg_idx = msg_idx;
    node->type = type;
    node->cmd = cmd;
    node->n_data = n_data;
    if (data == NULL)
    {
        node->data = NULL;
    }
    else
    {
        node->data = (char**)malloc(sizeof(char*) * n_data);
        if (node->data == NULL)
        {
            LOG_FATAL("malloc error: %s", strerror(errno));
            free(node);
            return NULL;
        }

        for (size_t i = 0; i < n_data; i++)
        {
            node->data[i] = data[i] ? strdup(data[i]) : NULL;
        }
    }
    node->next = NULL;

    return node;
}

void    remove_node(t_node* node)
{
    if (node == NULL)
    {
        return;
    }

    if (node->data)
    {
        for (size_t i = 0; i < node->n_data; i++)
        {
            free(node->data[i]);
        }

        free(node->data);
    }

    free(node);
}

t_queue*    create_queue(void)
{
    t_queue*    queue;

    queue = (t_queue*)malloc(sizeof(t_queue));
    if (!queue)
    {
        LOG_FATAL("malloc error: %s", strerror(errno));
        return NULL;
    }

    queue->head = NULL;
    queue->size = 0;
    queue->running = true;
    pthread_mutex_init(&queue->queue_mutex, NULL);
    pthread_cond_init(&queue->queue_cond, NULL);

    return queue;
}

void    clear_queue(t_queue* queue)
{
    t_node* temp;
    t_node* cur;

    if (queue == NULL)
    {
        return;
    }

    pthread_mutex_lock(&queue->queue_mutex);
    cur = queue->head;
    while (cur != NULL)
    {
        temp = cur->next;

        LOG_TRACE("cleaning node... %d", cur->client_fd);
        remove_node(cur);

        cur = temp;
    }
    queue->head = NULL;
    pthread_mutex_unlock(&queue->queue_mutex);

    pthread_mutex_destroy(&queue->queue_mutex);
    pthread_cond_destroy(&queue->queue_cond);
    free(queue);
}

bool    enqueue(t_queue* queue, t_node* node)
{
    t_node* cur;

    assert(queue != NULL);
    assert(node != NULL);

    if (queue->size > QUEUE_SIZE)
    {
        LOG_ERROR("max queue size reached");
        return false;
    }

    pthread_mutex_lock(&queue->queue_mutex);
    cur = queue->head;

    if (cur == NULL)
    {
        queue->head = node;
        queue->size = 1;
    }
    else
    {
        while (cur->next != NULL)
        {
            cur = cur->next;
        }

        cur->next = node;
        queue->size++;
    }

    pthread_cond_signal(&queue->queue_cond);
    pthread_mutex_unlock(&queue->queue_mutex);

    return true;
}

t_node* dequeue(t_queue* queue)
{
    t_node* cur;

    assert(queue != NULL);

    pthread_mutex_lock(&queue->queue_mutex);
    while (queue->head == NULL)
    {
        LOG_DEBUG("waiting for the signal...");

        /* Queue got the signal when enqueue performed 
         * and monitor flag is changed
         */
        pthread_cond_wait(&queue->queue_cond, &queue->queue_mutex);

        if (queue->running == false)
        {
            LOG_DEBUG("running is false, exit from queue");
            pthread_mutex_unlock(&queue->queue_mutex);
            return NULL;
        }
    }

    cur = queue->head;
    if (cur == NULL)
    {
        LOG_ERROR("queue head is empty");
        return NULL;
    }

    queue->head = cur->next;
    queue->size--;

    LOG_TRACE("queue size after dequeue: %ld", queue->size);
    pthread_mutex_unlock(&queue->queue_mutex);

    return cur;
}

