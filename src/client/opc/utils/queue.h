// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#ifndef __QUEUE_H__
# define __QUEUE_H__

# include "../opclient.h"
# include "../../../message/message.pb-c.h"

# define QUEUE_SIZE 64

typedef struct s_node
{
    int             client_fd;
    Status          status;
    CmdType         type;
    Cmd             cmd;
    int             msg_idx;
    size_t          n_data;
    char**          data;
    struct s_node*  next;
} t_node;

typedef struct s_queue
{
    t_node*         head;
    size_t          size;
    bool            running;
    pthread_mutex_t queue_mutex;
    pthread_cond_t  queue_cond;
} t_queue;

t_node*     create_node(int client_fd, Status status, CmdType type, Cmd command,
                        int msg_idx, size_t n_data, char** data);

void        remove_node(t_node* node);
t_queue*    create_queue(void);
void        remove_queue(t_queue* queue);
void        flush_queue(t_queue* queue);
bool        enqueue(t_queue* queue, t_node* node);
t_node*     dequeue(t_queue* queue);
t_node*     dequeue_conditional(t_queue* queue, int msg_idx);
bool        get_queue_running(t_queue* queue);

#endif /* __QUEUE_H__ */

