// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#include "response.h"

static bool set_status(OP_STATUS* status, t_node* node);

OP_STATUS*  get_command_response(OP_CLIENT* client)
{
    t_node*     node;
    bool        readed;
    OP_STATUS*  status;
    int         ret;

    assert(client != NULL);

    status = opc_create_status();
    if (status == NULL)
    {
        LOG_ERROR("couldn't create status");
        return NULL;
    }

    readed = false;
    while (get_monitor_play(client->monitor) == true)
    {
        node = dequeue_conditional(client->cmd_queue, client->msg_idx);
        if (node == NULL)
        {
            LOG_DEBUG("node is null, break loop");
            break;
        }

        if (node->msg_idx == client->msg_idx)
        {
            readed = true;
            ret = set_status(status, node);
            if (ret == false)
            {
                readed = false;
            }
            remove_node(node);
            break;
        }

        LOG_TRACE("skip diff node: %d, msg_idx: %d", node->msg_idx, 
                                                     client->msg_idx);
        remove_node(node);
    }

    LOG_TRACE("response: %d, %d, %d, %ld, %p", status->status, status->type, 
                                               status->msg_idx, status->n_data,
                                               status->data);

    if (readed == false)
    {
        opc_free_status(status);
        return NULL;
    }

    return status;
}

OP_STATUS*  get_message_response(OP_CLIENT* client)
{
    t_node*     node;
    OP_STATUS*  status;
    int         ret;

    assert(client != NULL);

    status = opc_create_status();
    if (status == NULL)
    {
        LOG_ERROR("couldn't create status");
        return NULL;
    }

    node = dequeue(client->msg_queue);
    if (node == NULL)
    {
        LOG_DEBUG("node is null, break loop");
        opc_free_status(status);
        return NULL;
    }

    ret = set_status(status, node);
    if (ret == false)
    {
        LOG_ERROR("couldn't set status");
        opc_free_status(status);
        remove_node(node);
        return NULL;
    }

    LOG_TRACE("response: %d, %d, %d, %ld, %p", status->status, status->type, 
                                               status->msg_idx, status->n_data,
                                               status->data);

    remove_node(node);

    return status;
}

static bool set_status(OP_STATUS* status, t_node* node)
{
    assert(status != NULL);
    assert(node != NULL);

    status->status = node->status;
    status->type = node->type;
    status->cmd = node->cmd;
    status->msg_idx = node->msg_idx;
    status->n_data = node->n_data;
    status->data = (char**)malloc(sizeof(char*) * status->n_data);
    if (status->data == NULL)
    {
        LOG_FATAL("malloc error: %s", strerror(errno));
        return false;
    }

    for (size_t i = 0; i < node->n_data; i++)
    {
        status->data[i] = node->data[i] == NULL ? NULL : strdup(node->data[i]);
    }

    return true;
}

