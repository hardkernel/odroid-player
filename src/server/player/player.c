// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include "player.h"

#define GST_LAUNCH_BIN  "/usr/bin/gst-launch-1.0"

t_player*   g_player;

static void*    run_player(void* data);
static pid_t    spawn_child(const char* uri, int log_level);
static bool     forward_playlist(t_player* player);
static bool     backward_playlist(t_player* player);

/* ---------------- signal handling ---------------- */

static void interrupt_handler(int signum)
{
    if (signum == SIGINT)
    {
        LOG_INFO("interrupt hander called");
        unlink(SERVER_SOCKET_PATH);
        start_watchdog_timer(g_player);
        set_player_running(g_player, false);
        stop_watchdog_timer(g_player);
    }
}

/* Capture SIGCHLD signal when child dies.
 * This makes parent wake when parent is in blocking state.
 * (waitpid, sleep) */
static void sigchld_handler(int signum)
{
    (void)signum;
}

/* ---------------- main functions ---------------- */

t_player*   create_player(bool is_loop, int log_level)
{
    t_player*       player;
    t_playlist*     playlist;
    t_client_pool*  client_pool;
    t_queue*        queue;

    set_log_level(log_level);

    player = (t_player*)malloc(sizeof(t_player));
    if (player == NULL)
    {
        LOG_FATAL("malloc error: %s", strerror(errno));
        return NULL;
    }

    playlist = create_playlist();
    if (playlist == NULL)
    {
        LOG_ERROR("couldn't create new playelist");
        free(player);
        return NULL;
    }

    client_pool = create_client_pool();
    if (client_pool == NULL)
    {
        LOG_ERROR("couldn't create new client pool");
        free(player);
        clear_playlist(playlist);
        return NULL;
    }

    queue = create_queue();
    if (queue == NULL)
    {
        LOG_ERROR("couldn't create new queue");
        free(player);
        clear_playlist(playlist);
        clear_client_pool(client_pool);
        return NULL;
    }

    player->playlist = playlist;
    player->client_pool = client_pool;
    player->queue = queue;
    player->agmp_handle = NULL;
    player->serv_thread_id = 0;
    player->resp_thread_id = 0;
    player->strm_thread_id = 0;
    player->wtdg_thread_id = 0;
    player->play_thread_id = 0;
    player->running = false;
    player->loop = is_loop;
    player->serv_maxfd = 0;
    player->strm_maxfd = 0;
    player->prev_resp = NULL;

    player->child_pid = 0;
    player->child_paused = false;
    player->child_stopped = false;
    player->paused_offset_us = 0;
    player->pending_command = PLAY_CMD_NONE;
    player->log_level = log_level;
    memset(&player->child_start_time, 0, sizeof(player->child_start_time));
    memset(&player->pause_started, 0, sizeof(player->pause_started));

    FD_ZERO(&player->serv_set);
    FD_ZERO(&player->strm_set);

    pthread_mutex_init(&player->player_mutex, NULL);
    pthread_mutex_init(&player->play_mutex, NULL);
    pthread_cond_init(&player->play_cond, NULL);

    /* For signal handler */
    g_player = player;

    return player;
}

bool    prepare_player(t_player* player, char** filelist, AGMP_CALLBACK cb)
{
    size_t  filenum;

    (void)cb;
    assert(player != NULL);

    for (size_t i = 0; filelist[i] != NULL; i++)
    {
        filenum = get_playlist_filenum(player->playlist);
        if (filenum >= MAX_FILENUM)
        {
            LOG_ERROR("too many files in the playlist");
            return false;
        }

        append_playlist(player->playlist, filelist[i]);
    }

    filenum = get_playlist_filenum(player->playlist);
    if (filenum == 0)
    {
        LOG_ERROR("no files to play");
        return false;
    }

    for (size_t i = 0; filelist[i] != NULL; i++)
    {
        free(filelist[i]);
    }

    return true;
}

bool    start_player(t_player* player)
{
    struct sigaction    sa;
    sigset_t            mask;
    int                 ret;

    assert(player != NULL);

    set_player_running(player, true);

    /* Parent signals */
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = interrupt_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);
    sigaction(SIGPIPE, &sa, NULL);

    /* Child signals */
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = sigchld_handler;
    sigaction(SIGCHLD, &sa, NULL);

    sigemptyset(&mask);
    sigaddset(&mask, SIGPIPE);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    ret = pthread_create(&player->strm_thread_id, NULL, run_stream, player);
    if (ret != 0)
    {
        LOG_ERROR("couldn't create stream thread");
        return false;
    }

    ret = pthread_create(&player->serv_thread_id, NULL, run_server, player);
    if (ret != 0)
    {
        LOG_ERROR("couldn't create server thread");
        return false;
    }

    ret = pthread_create(&player->resp_thread_id, NULL, run_response, player);
    if (ret != 0)
    {
        LOG_ERROR("couldn't create response thread");
        return false;
    }

    ret = pthread_create(&player->wtdg_thread_id, NULL, run_watchdog, player);
    if (ret != 0)
    {
        LOG_ERROR("couldn't create watchdog thread");
        return false;
    }
    pthread_detach(player->wtdg_thread_id);

    ret = pthread_create(&player->play_thread_id, NULL, run_player, player);
    if (ret != 0)
    {
        LOG_ERROR("couldn't create play thread");
        return false;
    }

    return true;
}

void    clear_player(t_player* player)
{
    assert(player != NULL);

    set_player_running(player, false);

    /* Ensure any running child got signals and exited. */
    pthread_mutex_lock(&player->play_mutex);
    if (player->child_pid > 0)
    {
        if (player->child_paused)
        {
            kill(player->child_pid, SIGCONT);
            player->child_paused = false;
        }
        kill(player->child_pid, SIGTERM);
    }
    pthread_cond_signal(&player->play_cond);
    pthread_mutex_unlock(&player->play_mutex);

    /* Ensure play thread is joined. */
    if (player->play_thread_id != 0)
    {
        pthread_join(player->play_thread_id, NULL);
        player->play_thread_id = 0;
    }

    clear_playlist(player->playlist);
    clear_client_pool(player->client_pool);
    clear_queue(player->queue);

    if (player->prev_resp != NULL)
    {
        remove_node(player->prev_resp);
    }

    pthread_cond_destroy(&player->play_cond);
    pthread_mutex_destroy(&player->play_mutex);
    pthread_mutex_destroy(&player->player_mutex);

    free(player);
}

void    wait_player(t_player* player)
{
    assert(player != NULL);

    pthread_join(player->strm_thread_id, NULL);
    pthread_join(player->serv_thread_id, NULL);
    pthread_join(player->resp_thread_id, NULL);
    pthread_join(player->play_thread_id, NULL);
}

bool    get_player_running(t_player* player)
{
    bool    running;

    assert(player != NULL);

    pthread_mutex_lock(&player->player_mutex);
    running = player->running;
    pthread_mutex_unlock(&player->player_mutex);

    return running;
}

void    set_player_running(t_player* player, bool flag)
{
    assert(player != NULL);

    /* If flag is false, player starts preparing to stop */
    pthread_mutex_lock(&player->player_mutex);
    player->running = flag;
    pthread_mutex_unlock(&player->player_mutex);

    /* Send signal to waiting queue to inform flag is changed */
    pthread_mutex_lock(&player->queue->queue_mutex);
    player->queue->running = flag;
    pthread_cond_signal(&player->queue->queue_cond);
    pthread_mutex_unlock(&player->queue->queue_mutex);

    /* Wake play_loop if it is waiting in cond_wait. */
    pthread_mutex_lock(&player->play_mutex);
    pthread_cond_signal(&player->play_cond);
    pthread_mutex_unlock(&player->play_mutex);

    /* Send signal to pselect for same reason */
    if (flag == false)
    {
        if (player->strm_thread_id != 0)
        {
            pthread_kill(player->strm_thread_id, SIGUSR1);
        }
        if (player->serv_thread_id != 0)
        {
            pthread_kill(player->serv_thread_id, SIGUSR1);
        }
    }
}

/* Command interface between player and server. */
bool    command_player(t_player* player, t_player_cmd command)
{
    pid_t   child;

    assert(player != NULL);

    pthread_mutex_lock(&player->play_mutex);

    player->pending_command = command;
    pthread_cond_signal(&player->play_cond);

    child = player->child_pid;
    if (child > 0)
    {
        switch (command)
        {
            case PLAY_CMD_NEXT:
            case PLAY_CMD_PREV:
            case PLAY_CMD_STOP:
            case PLAY_CMD_REPLAY:
                if (player->child_paused)
                {
                    kill(child, SIGCONT);
                    player->child_paused = false;
                }
                kill(child, SIGTERM);
                break;

            case PLAY_CMD_PAUSE:
                if (!player->child_paused)
                {
                    kill(child, SIGSTOP);
                    player->child_paused = true;
                    gettimeofday(&player->pause_started, NULL);
                }
                break;

            case PLAY_CMD_RESUME:
                if (player->child_paused)
                {
                    struct timeval now;
                    long long pause_us;
                    gettimeofday(&now, NULL);
                    pause_us =
                        (long long)(now.tv_sec - player->pause_started.tv_sec)
                            * 1000000LL
                            + (now.tv_usec - player->pause_started.tv_usec);
                    player->paused_offset_us += pause_us;
                    kill(child, SIGCONT);
                    player->child_paused = false;
                }
                break;

            default:
                break;
        }
    }

    pthread_mutex_unlock(&player->play_mutex);
    return true;
}

long long   get_player_position(t_player* player)
{
    struct timeval  now;
    long long       delta_us;
    long long       paused_us;

    assert(player != NULL);

    pthread_mutex_lock(&player->play_mutex);
    if (player->child_pid <= 0
        || (player->child_start_time.tv_sec == 0
            && player->child_start_time.tv_usec == 0))
    {
        pthread_mutex_unlock(&player->play_mutex);
        return -1;
    }

    /* Calculate position based on wall-clock time. */
    gettimeofday(&now, NULL);
    delta_us = (long long)(now.tv_sec - player->child_start_time.tv_sec)
                    * 1000000LL
                    + (now.tv_usec - player->child_start_time.tv_usec);
    delta_us -= player->paused_offset_us;

    if (player->child_paused)
    {
        paused_us = (long long)(now.tv_sec - player->pause_started.tv_sec)
                        * 1000000LL
                        + (now.tv_usec - player->pause_started.tv_usec);
        delta_us -= paused_us;
    }
    pthread_mutex_unlock(&player->play_mutex);

    if (delta_us < 0)
    {
        delta_us = 0;
    }

    /* Convert microseconds to nanoseconds */
    return delta_us * 1000LL;
}

/* ---------------- internals ---------------- */

static pid_t    spawn_child(const char* uri, int log_level)
{
    pid_t   pid;

    if (uri == NULL)
    {
        return -1;
    }

    pid = fork();
    if (pid < 0)
    {
        LOG_ERROR("fork failed: %s", strerror(errno));
        return -1;
    }

    if (pid == 0)
    {
        /* Restore default signal to child process. Child process needs 
         * default signal behavior to handle gst-launch cleanly. */
        signal(SIGTERM, SIG_DFL);
        signal(SIGINT,  SIG_DFL);
        signal(SIGCHLD, SIG_DFL);
        signal(SIGPIPE, SIG_DFL);
        signal(SIGUSR1, SIG_DFL);
        signal(SIGUSR2, SIG_DFL);

        sigset_t mask;
        sigemptyset(&mask);
        pthread_sigmask(SIG_SETMASK, &mask, NULL);

        setenv("XDG_RUNTIME_DIR", "/run/user/1000", 1);
        setenv("GST_DEFAULT_V4L2_BUF_MODE", "DMA_BUF_IMPORT", 1);
        setenv("V4L2_SET_AMLOGIC_DW_MODE", "1", 1);

        char uri_arg[BUFFER_SIZE];
        snprintf(uri_arg, sizeof(uri_arg), "uri=%s", uri);

        char* const argv[] = {
            "gst-launch-1.0",
            "-e",
            "playbin",
            uri_arg,
            "video-sink=amlvideosink show-first-frame-asap=true",
            "audio-sink=amlhalasink",
            NULL
        };

        /* Send child stdout/stderr to /dev/null on higher log_level
         * so it does not interfere the parent's logs. */
        if (log_level >= 4)
        {
            int devnull = open("/dev/null", O_WRONLY);
            if (devnull >= 0)
            {
                dup2(devnull, 1);
                dup2(devnull, 2);
                close(devnull);
            }
        }

        execvp(GST_LAUNCH_BIN, argv);

        _exit(127);
    }

    return pid;
}

static bool forward_playlist(t_player* player)
{
    int    cur;
    size_t filenum;
    char*  uri;

    filenum = get_playlist_filenum(player->playlist);
    cur = get_playlist_cur_idx(player->playlist);
    if (++cur >= filenum)
    {
        if (!player->loop)
        {
            return false;
        }
        cur = 0; /* loop */
    }

    set_playlist_cur_idx(player->playlist, cur);

    uri = get_playlist_uri(player->playlist, cur);
    if (uri != NULL)
    {
        send_broadcast_message(player->client_pool, uri,
                               -1, STATUS__BRDINFO, CMD__URI);
    }

    return true;
}

static bool backward_playlist(t_player* player)
{
    int    cur;
    char*  uri;

    cur = get_playlist_cur_idx(player->playlist);
    if (cur-- <= 0)
    {
        return false;
    }

    set_playlist_cur_idx(player->playlist, cur);

    uri = get_playlist_uri(player->playlist, cur);
    if (uri != NULL)
    {
        send_broadcast_message(player->client_pool, uri,
                               -1, STATUS__BRDINFO, CMD__URI);
    }
    return true;
}

/* Player thread for continuous media playback.
 * Playback is handled by child process.
 */
static void* run_player(void* data)
{
    t_player*       player = (t_player*)data;
    int             status;
    pid_t           pid;
    char*           uri;
    int             cur;
    t_player_cmd    pending;
    t_player_cmd    wakeup;

    LOG_INFO("player thread is running...");

    while (get_player_running(player) == true)
    {
        /* Child is not created yet. Waiting for the command. */
        pthread_mutex_lock(&player->play_mutex);
        while (player->child_stopped
               && player->pending_command == PLAY_CMD_NONE
               && get_player_running(player))
        {
            pthread_cond_wait(&player->play_cond, &player->play_mutex);
        }

        if (!get_player_running(player))
        {
            pthread_mutex_unlock(&player->play_mutex);
            break;
        }

        /* Case: STOP -> NEXT | PREV */
        if (player->child_stopped && player->pending_command != PLAY_CMD_NONE)
        {
            wakeup = player->pending_command;
            player->pending_command = PLAY_CMD_NONE;
            player->child_stopped = false;
            pthread_mutex_unlock(&player->play_mutex);

            if (wakeup == PLAY_CMD_NEXT)
            {
                forward_playlist(player);
            }
            else if (wakeup == PLAY_CMD_PREV)
            {
                backward_playlist(player);
            }
        }
        else
        {
            pthread_mutex_unlock(&player->play_mutex);
        }

        cur = get_playlist_cur_idx(player->playlist);
        uri = get_playlist_uri(player->playlist, cur);
        if (uri == NULL)
        {
            LOG_ERROR("playlist returned NULL uri at index %d", cur);
            break;
        }

        LOG_DEBUG("play loop: spawning child for uri=%s", uri);

        /* Create new child for playback */
        pid = spawn_child(uri, player->log_level);
        if (pid < 0)
        {
            LOG_ERROR("failed to spawn child for uri=%s", uri);
            usleep(500 * 1000); /* 500 ms */
            continue;
        }

        pthread_mutex_lock(&player->play_mutex);
        player->child_pid = pid;
        player->child_paused = false;
        player->paused_offset_us = 0;
        gettimeofday(&player->child_start_time, NULL);
        pthread_mutex_unlock(&player->play_mutex);

        /* Wait until child is finished or terminated. */
        while (true)
        {
            pid_t w = waitpid(pid, &status, 0);
            if (w == pid)
            {
                /* Child finished or terminated. */
                break;
            }
            if (w < 0 && errno == EINTR)
            {
                /* waitpit returns EINTR on SIGCHLD signals. 
                 * Keep looping on SIGCONT | SIGSTOP.
                 */
                continue;
            }
            if (w < 0)
            {
                LOG_ERROR("waitpid(%d) failed: %s", pid, strerror(errno));
                break;
            }
        }

        /* Cleanup old child state and store pending_comamnd for new child. */
        pthread_mutex_lock(&player->play_mutex);
        pending = player->pending_command;
        player->pending_command = PLAY_CMD_NONE;
        player->child_pid = 0;
        player->child_paused = false;
        memset(&player->child_start_time, 0,
               sizeof(player->child_start_time));
        pthread_mutex_unlock(&player->play_mutex);

        LOG_DEBUG("play loop: child %d exited (status=0x%x), command=%d",
                  (int)pid, status, (int)pending);

        if (!get_player_running(player))
        {
            break;
        }

        /* back to the top and blocked in cond_wait */
        if (pending == PLAY_CMD_STOP)
        {
            pthread_mutex_lock(&player->play_mutex);
            player->child_stopped = true;
            pthread_mutex_unlock(&player->play_mutex);
            continue;
        }

        /* Set previous media on playlist. */
        if (pending == PLAY_CMD_PREV)
        {
            backward_playlist(player);
            continue;
        }

        /* Set same media on playlist. */
        if (pending == PLAY_CMD_REPLAY)
        {
            continue;
        }

        /* Default action (EOS or PLAY_CMD_NEXT) 
         * Set next media on playlist.
         */
        if (!forward_playlist(player))
        {
            LOG_DEBUG("end of playlist, stopping");
            set_player_running(player, false);
            break;
        }
    }

    set_player_running(player, false);

    LOG_INFO("play loop thread finished");

    return NULL;
}

