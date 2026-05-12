/*
 * Copyright (C) 2021 Amlogic Corporation.
 * Copyright (C) 2025 Phillip Choi for Hardkernel.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

 /*
  * This file has been modified by Phillip Choi (phillip.choi@hardkernel.com).
  * Original copyright belongs to Amlogic Corporation under Apache License 2.0.
  */

#include <ctype.h>
#include "player/player.h"

#define PROGRAM_NAME "odroid-player"

static void print_usage(void)
{
    printf("Usage: %s [OPTIONS] FILE1 [FILE2] [FILE3] ...", PROGRAM_NAME);
    printf("\n\n");
    printf("%s\n\n", "You must provide at least one filename to play.");
    printf("\t--file=FILE\t\tset playlist using a text file\n");
    printf("\t--help\t\t\tprint this help\n");
    printf("\t--loop=FALSE\t\tdisable loop (default is true)\n");
    printf("\t--log-level=LEVEL\tset log level. 0 is the most verbose, "
           "6 is to disable log message (default is 6)");
    printf("\n\n");
}

static int  safe_sscanf(const char* data, const char* format, char* buffer,
                        size_t buf_len)
{
    char    safe_format[32];

    if (buf_len == 0)
    {
        return 0;
    }

    snprintf(safe_format, sizeof(safe_format), format, (int)(buf_len - 1));

    return sscanf(data, safe_format, buffer);
}

static void to_lower(char* buffer)
{
    size_t  buf_len;

    buf_len = strlen(buffer);
    if (buf_len == 0)
    {
        return;
    }

    for (int i = 0; i < buf_len; i++)
    {
        buffer[i] = tolower(buffer[i]);
    }
}

static void free_filelist(char** filelist)
{
    size_t  idx;

    if (filelist == NULL)
    {
        return;
    }

    idx = 0;
    while (filelist[idx] != NULL)
    {
        free(filelist[idx++]);
    }
}

static void handle_filelist(char* file, char** filelist)
{
    static size_t   file_idx;
    char            buffer[BUFFER_SIZE];

    if (file_idx > MAX_FILENUM || file == NULL)
    {
        return;
    }

    if (access(file, F_OK) != 0)
    {
        printf("couldn't access %s: %s\n", file, strerror(errno));
        return;
    }

    snprintf(buffer, sizeof(buffer), "file://%s", file);
    filelist[file_idx++] = strdup(buffer);
}

static void handle_option(char* arg, char** filelist, bool* file_flag,
                          bool* is_loop, int* level)
{
    FILE*   file;
    char    file_name[BUFFER_SIZE];
    char    file_buff[BUFFER_SIZE];
    char    loop[8];
    int     log_level;

    if (strncmp(arg, "--help", 7) == 0)
    {
        print_usage();
        exit(EXIT_SUCCESS);
    }
    else if (safe_sscanf(arg, "--loop=%%%ds", loop, sizeof(loop)) > 0)
    {
        to_lower(loop);
        if (strncmp(loop, "false", 6) == 0 || strncmp(loop, "0", 2) == 0)
        {
            *is_loop = false;
        }
    }
    else if (sscanf(arg, "--log-level=%d", &log_level) > 0)
    {
        if (0 <= log_level && log_level <= 6)
        {
            *level = log_level;
        }
    }
    else if (safe_sscanf(arg, "--file=%%%ds", file_name, sizeof(file_name)) > 0)
    {
        *file_flag = true;

        file = fopen(file_name, "rt");
        if (file == NULL)
        {
            printf("couldn't open file %s: %s\n", file_name, strerror(errno));
            return;
        }

        while (fgets(file_buff, BUFFER_SIZE, file) != NULL)
        {
            handle_filelist(strtok(file_buff, "\n"), filelist);
        }

        fclose(file);
    }
    else
    {
        printf("Invalid option\n");
        exit(EXIT_FAILURE);
    }
}

static void handle_argument(int argc, char* argv[], bool* is_loop, int* level, 
                            char** filelist)
{
    bool    file_flag;

    file_flag = false;
    for (int i = 1; i < argc; i++)
    {
        if (strncmp(argv[i], "--", 2) == 0)
        {
            handle_option(argv[i], filelist, &file_flag, is_loop, level);
        }
    }

    if (file_flag == true)
    {
        return;
    }

    for (int i = 1; i < argc; i++)
    {
        if (strncmp(argv[i], "--", 2))
        {
            handle_filelist(argv[i], filelist);
        }
    }
}

int main(int argc, char **argv)
{
    t_player*       player;
    char*           filelist[MAX_FILENUM];
    bool            is_loop;
    int             log_level, ret;

    if (argc < 2)
    {
        printf("Usage: %s [OPTIONS] FILE1 [FILE2] [FILE3] ...", PROGRAM_NAME);
        printf("\n\n");
        printf("%s\n\n", "You must provide at least one filename to play.");
        return EXIT_FAILURE;
    }

    is_loop = true;
    log_level = QUIET;
    memset(filelist, 0, sizeof(filelist));
    handle_argument(argc, argv, &is_loop, &log_level, filelist);

    if (geteuid() != 0)
    {
        printf("You should run this player as a sudo.\n\n");
        free_filelist(filelist);
        return EXIT_FAILURE;
    }

    if (access(SERVER_SOCKET_PATH, F_OK) == 0)
    {
        printf("Player is already running, or socket is not removed.\n");
        free_filelist(filelist);
        return EXIT_FAILURE;
    }

    player = create_player(is_loop, log_level);
    if (player == NULL)
    {
        LOG_ERROR("couldn't create player");
        free_filelist(filelist);
        return EXIT_FAILURE;
    }

    ret = prepare_player(player, filelist, agmp_message_callback);
    if (ret == false)
    {
        LOG_ERROR("couldn't prepare player");
        free_filelist(filelist);
        goto close_player;
    }

    ret = start_player(player);
    if (ret == false)
    {
        LOG_ERROR("couldn't start player");
        goto close_player;
    }

    wait_player(player);

close_player:
    clear_player(player);

    LOG_INFO("player quit\n");

    return EXIT_SUCCESS;
}

