// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#include "player.h"

t_player*   g_player;

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

t_player*   create_player(bool is_loop, int log_level)
{
    t_player*       player;
    t_playlist*     playlist;
    t_client_pool*  client_pool;
    t_queue*        queue;
    AGMP_HANDLE     agmp_handle;

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

    agmp_handle = agmp_init();
    if (agmp_handle == NULL)
    {
        LOG_ERROR("couldn't initialize agmp handle");
        free(player);
        clear_playlist(playlist);
        clear_client_pool(client_pool);
        clear_queue(queue);
        return NULL;
    }

    player->playlist = playlist;
    player->client_pool = client_pool;
    player->queue = queue;
    player->agmp_handle = agmp_handle;
    player->serv_thread_id = 0;
    player->resp_thread_id = 0;
    player->strm_thread_id = 0;
    player->wtdg_thread_id = 0;
    player->running = false;
    player->loop = is_loop;
    player->serv_maxfd = 0;
    player->strm_maxfd = 0;
    player->prev_resp = NULL;

    FD_ZERO(&player->serv_set);
    FD_ZERO(&player->strm_set);

    pthread_mutex_init(&player->player_mutex, NULL);

    /* for signal handler */
    g_player = player;

    return player;
}

/* Player must set the agmp callback function to control the media stream */
bool    prepare_player(t_player* player, char** filelist, AGMP_CALLBACK cb)
{
    size_t  filenum;
    char*   uri;

    assert(player != NULL);
    assert(cb != NULL);

    /* Set playlist */
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

    /* Register callback function to control the media stream */
    aamp_register_events(player->agmp_handle, cb, player);

    /* Prepare the first media to play */
    uri = get_playlist_uri(player->playlist, 0);
    if (uri == NULL)
    {
        LOG_ERROR("couldn't get playlist uri");
        return false;
    }
    agmp_set_uri(player->agmp_handle, uri);

    /* This envs required to use amlvideosink in odroid-c5 */
    setenv("XDG_RUNTIME_DIR", "/run/user/1000", 1);
    setenv("GST_DEFAULT_V4L2_BUF_MODE", "DMA_BUF_IMPORT", 1);
    setenv("V4L2_SET_AMLOGIC_DW_MODE", "1", 1);

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
    char                temp_buf[AGMP_BUFFER_SIZE];
    int                 ret;

    assert(player != NULL);

    set_player_running(player, true);

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = interrupt_handler;
    sigaction(SIGINT, &sa, NULL);   /* set handler for handling interrupt */
    sigaction(SIGUSR1, &sa, NULL);  /* set handler for other signals to use */
    sigaction(SIGUSR2, &sa, NULL);   
    sigaction(SIGPIPE, &sa, NULL);

    sigemptyset(&mask);
    sigaddset(&mask, SIGPIPE);  /* used by handling recv/send error */
    sigaddset(&mask, SIGUSR1);  /* used by stream/server/response threads */
    sigaddset(&mask, SIGUSR2);  /* used by watchdog thread */
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

    start_watchdog_timer(player);
    ret = agmp_prepare(player->agmp_handle, temp_buf);
    stop_watchdog_timer(player);
    if (ret > 0)
    {
        LOG_ERROR("%s", temp_buf);
        return false;
    }

    start_watchdog_timer(player);
    ret = agmp_play(player->agmp_handle, temp_buf);
    stop_watchdog_timer(player);
    if (ret > 0)
    {
        LOG_ERROR("%s", temp_buf);
        return false;
    }

    return true;
}

void    clear_player(t_player* player)
{
    assert(player != NULL);

    /* Set running false again for threads which didn't receive stop signal yet.
     * Watchdog thread is detatched, so it will be removed with program end.
     */
    set_player_running(player, false);

    clear_playlist(player->playlist);
    clear_client_pool(player->client_pool);
    clear_queue(player->queue);

    start_watchdog_timer(player);
    agmp_exit(player->agmp_handle);
    stop_watchdog_timer(player);

    if (player->prev_resp != NULL)
    {
        remove_node(player->prev_resp);
    }

    pthread_mutex_destroy(&player->player_mutex);

    free(player);
}

void    player_wait(t_player* player)
{
    assert(player != NULL);

    /* Wait all threads for playing media continuously.
     * If error occured, all threads will be exited at the same time.
     */
    pthread_join(player->strm_thread_id, NULL);
    pthread_join(player->serv_thread_id, NULL);
    pthread_join(player->resp_thread_id, NULL);
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

