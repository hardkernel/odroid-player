// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#include "queue.h"

t_node* create_node(int client_fd, Status status, CmdType type, Cmd command,
                    int msg_idx, size_t n_data, char** data)
{
    t_node*    node;

    node = (t_node*)malloc(sizeof(t_node));
    if (!node)
    {
        LOG_FATAL("malloc error: %s", strerror(errno));
        return NULL;
    }

    node->client_fd = client_fd;
    node->status = status;
    node->type = type;
    node->cmd = command;
    node->msg_idx = msg_idx;
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

void    remove_queue(t_queue* queue)
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

        LOG_TRACE("cleaning msg_idx %d node...", cur->msg_idx);
        remove_node(cur);

        cur = temp;
    }
    pthread_mutex_unlock(&queue->queue_mutex);

    pthread_mutex_destroy(&queue->queue_mutex);
    pthread_cond_destroy(&queue->queue_cond);
    free(queue);
}

void    flush_queue(t_queue* queue)
{
    t_node* temp;
    t_node* cur;

    assert(queue != NULL);
    LOG_DEBUG("flushing queue...");

    pthread_mutex_lock(&queue->queue_mutex);
    cur = queue->head;
    while (cur != NULL)
    {
        temp = cur->next;

        remove_node(cur);

        cur = temp;
    }
    queue->head = NULL;
    queue->size = 0;

    pthread_cond_signal(&queue->queue_cond);
    pthread_mutex_unlock(&queue->queue_mutex);
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
        LOG_DEBUG("waiting for the message...");

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
        pthread_mutex_unlock(&queue->queue_mutex);
        return NULL;
    }

    queue->head = cur->next;
    queue->size--;

    LOG_TRACE("queue size after dequeue: %ld", queue->size);
    pthread_mutex_unlock(&queue->queue_mutex);

    return cur;
}

t_node* dequeue_conditional(t_queue* queue, int msg_idx)
{
    t_node* cur;

    assert(queue != NULL);

    pthread_mutex_lock(&queue->queue_mutex);
    while (queue->head == NULL)
    {
        LOG_DEBUG("waiting for the message...");

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
        pthread_mutex_unlock(&queue->queue_mutex);
        return NULL;
    }

    if (cur->next)
    {
        if (cur->next->msg_idx > msg_idx)
        {
            LOG_TRACE("another msg_idx found: %d, %d", msg_idx, 
                                                       cur->next->msg_idx);
            pthread_mutex_unlock(&queue->queue_mutex);
            return NULL;
        }
    }

    queue->head = cur->next;
    queue->size--;

    LOG_TRACE("queue size after dequeue: %ld", queue->size);
    pthread_mutex_unlock(&queue->queue_mutex);

    return cur;
}

bool    get_queue_running(t_queue* queue)
{
    bool    running;

    assert(queue != NULL);

    pthread_mutex_lock(&queue->queue_mutex);
    running = queue->running;
    pthread_mutex_unlock(&queue->queue_mutex);

    return running;
}

