// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#include "watchdog.h"

/* Watchdog thread for setting timeout to execute function */
void*   run_watchdog(void* data)
{
    t_player*               player;
    sigset_t                mask;
    siginfo_t               info;
    struct timespec         tv;
    volatile sig_atomic_t   triggered;
    int                     ret, temp_errno;

    assert(data != NULL);

    player = (t_player*)data;

    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);

    tv.tv_sec = 0;
    tv.tv_nsec = 100000000L; /* 100ms */

    LOG_INFO("watchdog thread is running...");

    triggered = false;
    while (true)
    {
loop:
        ret = sigwaitinfo(&mask, &info);
        if (ret < 0)
        {
            LOG_ERROR("sigwaitinfo error: %s", strerror(errno));
            break;
        }
        else if (info.si_signo != SIGUSR1)
        {
            goto loop;
        }

        for (int i = 0; i < WATCHDOG_TIMEOUT * 10; i++)
        {
            temp_errno = errno;
            ret = sigtimedwait(&mask, &info, &tv);
            if (ret < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    errno = temp_errno;
                    LOG_DEBUG("watchdog is watching...");
                    continue;
                }
                goto out;
            }
            else if (info.si_signo != SIGUSR2)
            {
                continue;
            }

            triggered = true;
            break;
        }

        if (triggered == false)
        {
            char    msg[WTDG_BUFFER_SIZE];

            sprintf(msg, "watchdog timeout: %s\n", strerror(errno));
            LOG_ERROR(msg);
            send_broadcast_message(player->client_pool, msg, -1,
                                   STATUS__ESERVER, CMD__NONE);
            unlink(SERVER_SOCKET_PATH);
            exit(EXIT_FAILURE);
        }

        triggered = false;
    }
out:
    LOG_INFO("watchdog thread finished");

    return NULL;
}

void    start_watchdog_timer(t_player* player)
{
    int ret;

    assert(player != NULL);
    assert(player->wtdg_thread_id != 0);

    errno = 0;

    ret = pthread_kill(player->wtdg_thread_id, SIGUSR1);
    if (ret > 0)
    {
        LOG_ERROR("pthread_kill error: %s\n", strerror(errno));
    }
}

void    stop_watchdog_timer(t_player* player)
{
    int ret;

    assert(player != NULL);
    assert(player->wtdg_thread_id != 0);

    ret = pthread_kill(player->wtdg_thread_id, SIGUSR2);
    if (ret > 0)
    {
        LOG_ERROR("pthread_kill error: %s\n", strerror(errno));
    }
}

