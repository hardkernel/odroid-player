// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#include "opc_utils.h"

/* This function is not thread-safe. please handle with mutex */
OP_STATUS*  opc_set_timeout(OP_CLIENT* client,
                            OP_STATUS* (*fn)(OP_CLIENT*))
{
    OP_STATUS*    status;

    assert(client != NULL);

    if (sigsetjmp(client->env, 1) == 0)
    {
        alarm(EXECUTE_TIMEOUT);
        status = fn(client);
        alarm(0);

        return status;
    }

    pthread_mutex_trylock(&client->cmd_queue->queue_mutex);
    pthread_mutex_unlock(&client->cmd_queue->queue_mutex);

    LOG_ERROR("command response timeout");
    return NULL;
}

/* This function is not thread-safe. please handle with mutex */
int opc_thread_timeout(OP_CLIENT* client, pthread_t tid,
                       int (*join)(pthread_t, void**))
{
    int ret;

    assert(client != NULL);

    if (sigsetjmp(client->env, 1) == 0)
    {
        alarm(EXECUTE_TIMEOUT);
        ret = join(tid, NULL);
        if (ret != 0)
        {
            LOG_ERROR("pthread_join failed: %s", strerror(errno));
        }
        alarm(0);

        return ret;
    }

    LOG_ERROR("thread join timeout");

    return errno;
}

