// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#include <termios.h>
#include "utils.h"

#define PROGRAM_NAME "odroid-player-client"

static struct termios   g_orig_term;

static void enable_raw_mode(void)
{
    struct termios  new_term;

    tcgetattr(STDIN_FILENO, &g_orig_term);

    new_term = g_orig_term;
    new_term.c_lflag &= ~(ICANON | ECHO);
    new_term.c_cc[VMIN] = 1;
    new_term.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSANOW, &new_term);
}

static void disable_raw_mode(void)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_term);
}

void    interrupt_handler(int signum)
{
    if (signum == SIGINT)
    {
        printf("signal handler called\n");
        disable_raw_mode();
        printf("\033[?25h");
        exit(EXIT_SUCCESS);
    }
}

static void print_usage(void)
{
    printf("Usage: %s [OPTIONS]", PROGRAM_NAME);
    printf("\n\n");
    printf("\t--help\t\t\tprint this help\n");
    printf("\t--log-level=LEVEL\tset log level. 0 is the most verbose, "
           "6 is to disable log message (default is 6)");
    printf("\n\n");
}

static void handle_argument(int argc, char* argv[], int* log_level)
{
    int level;

    for (int i = 1; i < argc; i++)
    {
        if (strncmp(argv[i], "--", 2) == 0)
        {
            if (strncmp(argv[i], "--help", 7) == 0)
            {
                print_usage();
                exit(EXIT_SUCCESS);
            }
            else if (sscanf(argv[i], "--log-level=%d", &level) > 0)
            {
                if (0 <= level && level <= 6)
                {
                    *log_level = level;
                }
            }
        }
    }
}

int main(int argc, char* argv[])
{
    int         ret, flags, log_level;
    char        c;
    OP_CLIENT*  client;
    OP_STATUS*  status;
    static bool paused;

    log_level = QUIET;
    handle_argument(argc, argv, &log_level);

    opc_set_log_level(log_level);

    client = opc_init();
    if (client == NULL)
    {
        perror("Failed to create client");
        return EXIT_FAILURE;
    }

    printf("connecting to server...\n");
    ret = opc_connect(client);
    if (ret == false)
    {
        perror("Failed to connect server");
        opc_destroy(client);
        return EXIT_FAILURE;
    }
    printf("server connected\n");

    ret = opc_prepare(client);
    if (ret == false)
    {
        perror("Failed to prepare client");
        opc_destroy(client);
        return EXIT_FAILURE;
    }

    set_my_async_receiver(client);

    signal(SIGINT, interrupt_handler);
    enable_raw_mode();

    printf(PROGRAM_NAME);
    printf(" key maps:\n");
    printf("\tSPACE: pause/play\n");
    printf("\ta: check previous executed command\n");
    printf("\tn: next\n");
    printf("\tb: prev\n");
    printf("\tESC: stop\n");
    printf("\tt: time\n");
    printf("\tq: quit\n");
    printf("\tu: check playing file uri\n");

    printf("\033[?25l");
    flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    
    paused = false;
    while (get_monitor_play(client->monitor) == true)
    {
        status = NULL;
        ret = read(STDIN_FILENO, &c, 1);
        if (ret < 0)
        {
            usleep(1000);
            continue;
        }

        switch (c)
        {
            case 'q':
            {
                printf("quit\n");
                set_monitor(client, false, STATUS__SUCCESS);
                break;
            }
            case ' ':
            {
                if (paused == true)
                {
                    paused = false;
                    status = opc_send_play(client);
                    if (status == NULL) { printf("status: null\n"); continue; }
                }
                else
                {
                    paused = true;
                    status = opc_send_pause(client);
                    if (status == NULL) { printf("status: null\n"); continue; }
                }
                printf("status: %d, %s\n", status->status, status->data[0]);
                break;
            }
            case 27: /* ESC */
            {
                paused = true;
                status = opc_send_stop(client);
                if (status == NULL) { printf("status: null\n"); continue; }
                printf("status: %d, %s\n", status->status, status->data[0]);
                break;
            }
            case 'a':
            {
                status = opc_send_ack(client);
                if (status == NULL) { printf("status: null\n"); continue; }
                printf("status: %d, %s\n", status->status, status->data[0]);
                break;
            }
            case 'n':
            {
                status = opc_send_next(client);
                if (status == NULL) { printf("status: null\n"); continue; }
                printf("status: %d, %s\n", status->status, status->data[0]);
                break;
            }
            case 'b':
            {
                status = opc_send_prev(client);
                if (status == NULL) { printf("status: null\n"); continue; }
                printf("status: %d, %s\n", status->status, status->data[0]);
                break;
            }
            case 't':
            {
                status = opc_recv_time(client);
                if (status == NULL) { printf("status: null\n"); continue; }
                printf("status: %d, %s %s\n", status->status, status->data[0],
                                              status->data[1]);
                my_print_stream(client, status->data[0], status->data[1]);
                break;
            }
            case 'u':
            {
                status = opc_recv_uri(client);
                if (status == NULL) { printf("status: null\n"); continue; }
                printf("status: %d, %s\n", status->status, status->data[0]);
                break;
            }
            default:
                break;
        }

        if (status != NULL && status->status == STATUS__ESERVER)
        {
            printf("server error detected in status. stopping...\n");
            set_monitor(client, false, STATUS__ESERVER);
        }

        opc_free_status(status);
    }

    printf("closing client...\n");
    printf("\033[?25h");
    fcntl(STDIN_FILENO, F_SETFL, flags);
    disable_raw_mode();

    opc_destroy(client);

    return EXIT_SUCCESS;
}

