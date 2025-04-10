// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/select.h>
#include "lib_loader.h"

#define DEV_INPUT_PATH "/dev/input"
#define DEV_EVENT_NAME "event"

#define PROGRAM_NAME "odroid-player-client-ir"
#define BUFFER_SIZE 512

static volatile sig_atomic_t    stop = 0;

typedef enum
{
    EV_KEY_UP = 103,
    EV_KEY_DOWN = 108,
    EV_KEY_LEFT = 105,
    EV_KEY_RIGHT = 106,
    EV_KEY_ENTER = 28,
    EV_KEY_HOME = 102,
    EV_KEY_LIST = 139
} e_key_input;

static void interrupt_handler(int signum)
{
    (void)signum;
    stop = 1;
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

static int  is_event_device(const struct dirent* dir)
{
    return strncmp(DEV_EVENT_NAME, dir->d_name, 5) == 0;
}

/* Grap ir_keypad which is not the common device */
static int  is_ir_keypad(char* name)
{
    return strncmp("ir_keypad", name, 9) == 0 &&
           strncmp("ir_keypad_common", name, 16) != 0;
}

static int  process_opc_command(int code, t_opc_handler* opc_handler)
{
    static int  paused = 0;
    OP_STATUS*  opc_status = NULL;

    switch (code)
    {
        case EV_KEY_LEFT:
        {
            opc_status = opc_handler->opc_send_prev(opc_handler->opc_client);
            if (opc_status != NULL)
            {
                printf("status: %d, %s\n", opc_handler->opc_status_get_status(opc_status),
                                           opc_handler->opc_status_get_data(opc_status)[0]);
            }
            break;
        }
        case EV_KEY_RIGHT:
        {
            opc_status = opc_handler->opc_send_next(opc_handler->opc_client);
            if (opc_status != NULL)
            {
                printf("status: %d, %s\n", opc_handler->opc_status_get_status(opc_status),
                                           opc_handler->opc_status_get_data(opc_status)[0]);
            }
            break;
        }
        case EV_KEY_ENTER:
        {
            if (paused == 1)
            {
                paused = 0;
                opc_status = opc_handler->opc_send_play(opc_handler->opc_client);
                if (opc_status != NULL)
                {
                    printf("status: %d, %s\n", opc_handler->opc_status_get_status(opc_status),
                                               opc_handler->opc_status_get_data(opc_status)[0]);
                }
            }
            else
            {
                paused = 1;
                opc_status = opc_handler->opc_send_pause(opc_handler->opc_client);
                if (opc_status != NULL)
                {
                    printf("status: %d, %s\n", opc_handler->opc_status_get_status(opc_status),
                                               opc_handler->opc_status_get_data(opc_status)[0]);
                }
            }
            break;
        }
        case EV_KEY_HOME:
        {
            printf("client stopped\n");
            stop = 1;
            break;
        }
        case EV_KEY_LIST:
        {
            paused = 1;
            opc_status = opc_handler->opc_send_stop(opc_handler->opc_client);
            if (opc_status != NULL)
            {
                printf("status: %d, %s\n", opc_handler->opc_status_get_status(opc_status),
                                           opc_handler->opc_status_get_data(opc_status)[0]);
            }
            break;
        }
        case EV_KEY_UP:
        case EV_KEY_DOWN:
        default:
            printf("not implemented\n");
            break;
    }

    opc_handler->opc_free_status(opc_status);

    return EXIT_SUCCESS;
}

static int  capture_ir_event(int fd, t_opc_handler* opc_handler)
{
    struct input_event  ev[64];
    unsigned int        type, code, value;
    int                 rd, ret;
    fd_set              rdfs;

    FD_ZERO(&rdfs);
    FD_SET(fd, &rdfs);

    while (!stop)
    {
        ret = select(fd + 1, &rdfs, NULL, NULL, NULL);
        if (ret < 0)
        {
            perror("select");
            ioctl(fd, EVIOCGRAB, (void*)0);
            close(fd);
            opc_handler->opc_destroy(opc_handler->opc_client);
            dlclose(opc_handler->dl_handle);
            return EXIT_FAILURE;
        }

        if (stop)
        {
            break;
        }

        rd = read(fd, ev, sizeof(ev));
        if (rd < (int)sizeof(struct input_event))
        {
            perror("read");
            printf("expected %d bytes, got %d\n",
                   (int)sizeof(struct input_event), rd);
            ioctl(fd, EVIOCGRAB, (void*)0);
            close(fd);
            opc_handler->opc_destroy(opc_handler->opc_client);
            dlclose(opc_handler->dl_handle);
            return EXIT_FAILURE;
        }

        for (size_t i = 0; i < rd / sizeof(struct input_event); i++)
        {
            type = ev[i].type;
            code = ev[i].code;
            value = ev[i].value;

            if (type == EV_SYN || type == EV_MSC || value == 0)
            {
                continue;
            }

            ret = process_opc_command(code, opc_handler);
            if (ret == EXIT_FAILURE)
            {
                printf("Failed to process opc command\n");
                ioctl(fd, EVIOCGRAB, (void*)0);
                close(fd);
                opc_handler->opc_destroy(opc_handler->opc_client);
                dlclose(opc_handler->dl_handle);
                return EXIT_FAILURE;
            }   
        }
    }

    ioctl(fd, EVIOCGRAB, (void*)0);
    close(fd);
    opc_handler->opc_destroy(opc_handler->opc_client);
    dlclose(opc_handler->dl_handle);
    return EXIT_SUCCESS;
}

static int  handle_ir_event(char* device_path, int log_level)
{
    int            fd, ret;
    t_opc_handler  opc_handler;

    if (device_path == NULL)
    {
        return EXIT_FAILURE;
    }

    fd = open(device_path, O_RDONLY);
    if (fd < 0)
    {
        perror("open");
        goto error;
    }

    signal(SIGINT, interrupt_handler);
    signal(SIGTERM, interrupt_handler);

    memset(&opc_handler, 0, sizeof(opc_handler));

    ret = load_opc_library(&opc_handler);
    if (ret == EXIT_FAILURE)
    {
        printf("Couldn't load dynamic library\n");
        goto error;
    }

    ret = set_opc_client(&opc_handler, log_level);
    if (ret == EXIT_FAILURE)
    {
        printf("Couldn't set opc client\n");
        dlclose(opc_handler.dl_handle);
        goto error;
    }

    free(device_path);

    return capture_ir_event(fd, &opc_handler);

error:
    close(fd);
    free(device_path);
    return EXIT_FAILURE;
}

static char*    find_ir_device(void)
{
    struct dirent** namelist;
    int             ndev, fd, flag;
    char            fname[BUFFER_SIZE];
    char            name[BUFFER_SIZE];

    ndev = scandir(DEV_INPUT_PATH, &namelist, is_event_device, alphasort);
    if (ndev <= 0)
    {
        return NULL;
    }

    flag = 0;
    for (int i = 0; i < ndev; i++)
    {
        if (flag == 1)
        {
            free(namelist[i]);
            continue;
        }

        memset(fname, 0, sizeof(fname));
        memset(name, 0, sizeof(name));

        snprintf(fname, sizeof(fname), "%s/%s", DEV_INPUT_PATH,
                 namelist[i]->d_name);

        fd = open(fname, O_RDONLY);
        if (fd < 0)
        {
            free(namelist[i]);
            continue;
        }

        ioctl(fd, EVIOCGNAME(sizeof(name)), name);
        if (is_ir_keypad(name))
        {
            flag = 1;
        }

        close(fd);
        free(namelist[i]);
    }

    free(namelist);

    if (flag != 1)
    {
        return NULL;
    }

    return strdup(fname);
}

int main(int argc, char* argv[])
{
    int     log_level;
    char*   device_path;


    log_level = 6; /* QUIET */
    handle_argument(argc, argv, &log_level);

    if (getuid() != 0)
    {
        printf("This program should run as a sudo\n");
        return EXIT_FAILURE;
    }

    device_path = find_ir_device();
    if (device_path == NULL)
    {
        printf("Couldn't find ir device\n");
        return EXIT_FAILURE;
    }

    printf("Found ir device: %s\n", device_path);

    return handle_ir_event(device_path, log_level);
}

