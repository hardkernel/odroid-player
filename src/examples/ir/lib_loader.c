// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#include "lib_loader.h"

int load_opc_library(t_opc_handler* opc_handler)
{
    opc_handler->dl_handle = dlopen(SYSTEM_LIB_PATH, RTLD_LAZY);
    if (opc_handler->dl_handle == NULL)
    {
        opc_handler->dl_handle = dlopen(OPC_LIB_PATH, RTLD_LAZY);
        if (opc_handler->dl_handle == NULL)
        {
            perror("dlopen");
            return EXIT_FAILURE;
        }
    }

    opc_handler->opc_init = dlsym(opc_handler->dl_handle, "opc_init");
    if (opc_handler->opc_init == NULL)
    {
        perror("dlsym");
        printf("opc_init: %s\n", dlerror());
        dlclose(opc_handler->dl_handle);
        return EXIT_FAILURE;
    }

    opc_handler->opc_prepare = dlsym(opc_handler->dl_handle, "opc_prepare");
    if (opc_handler->opc_prepare == NULL)
    {
        perror("dlsym");
        printf("opc_prepare: %s\n", dlerror());
        dlclose(opc_handler->dl_handle);
        return EXIT_FAILURE;
    }

    opc_handler->opc_connect = dlsym(opc_handler->dl_handle, "opc_connect");
    if (opc_handler->opc_connect == NULL)
    {
        perror("dlsym");
        printf("opc_connect: %s\n", dlerror());
        dlclose(opc_handler->dl_handle);
        return EXIT_FAILURE;
    }

    opc_handler->opc_destroy = dlsym(opc_handler->dl_handle, "opc_destroy");
    if (opc_handler->opc_destroy == NULL)
    {
        perror("dlsym");
        printf("opc_destroy: %s\n", dlerror());
        dlclose(opc_handler->dl_handle);
        return EXIT_FAILURE;
    }

    opc_handler->opc_send_play = dlsym(opc_handler->dl_handle, "opc_send_play");
    if (opc_handler->opc_send_play == NULL)
    {
        perror("dlsym");
        printf("opc_send_play: %s\n", dlerror());
        dlclose(opc_handler->dl_handle);
        return EXIT_FAILURE;
    }

    opc_handler->opc_send_stop = dlsym(opc_handler->dl_handle, "opc_send_stop");
    if (opc_handler->opc_send_stop == NULL)
    {
        perror("dlsym");
        printf("opc_send_stop: %s\n", dlerror());
        dlclose(opc_handler->dl_handle);
        return EXIT_FAILURE;
    }

    opc_handler->opc_send_pause = dlsym(opc_handler->dl_handle, "opc_send_pause");
    if (opc_handler->opc_send_pause == NULL)
    {
        perror("dlsym");
        printf("opc_send_pause: %s\n", dlerror());
        dlclose(opc_handler->dl_handle);
        return EXIT_FAILURE;
    }

    opc_handler->opc_send_next = dlsym(opc_handler->dl_handle, "opc_send_next");
    if (opc_handler->opc_send_next == NULL)
    {
        perror("dlsym");
        printf("opc_send_next: %s\n", dlerror());
        dlclose(opc_handler->dl_handle);
        return EXIT_FAILURE;
    }

    opc_handler->opc_send_prev = dlsym(opc_handler->dl_handle, "opc_send_prev");
    if (opc_handler->opc_send_prev == NULL)
    {
        perror("dlsym");
        printf("opc_send_prev: %s\n", dlerror());
        dlclose(opc_handler->dl_handle);
        return EXIT_FAILURE;
    }

    opc_handler->opc_send_ack = dlsym(opc_handler->dl_handle, "opc_send_ack");
    if (opc_handler->opc_send_ack == NULL)
    {
        perror("dlsym");
        printf("opc_send_ack: %s\n", dlerror());
        dlclose(opc_handler->dl_handle);
        return EXIT_FAILURE;
    }

    opc_handler->opc_recv_time = dlsym(opc_handler->dl_handle, "opc_recv_time");
    if (opc_handler->opc_recv_time == NULL)
    {
        perror("dlsym");
        printf("opc_recv_time: %s\n", dlerror());
        dlclose(opc_handler->dl_handle);
        return EXIT_FAILURE;
    }

    opc_handler->opc_recv_amsg = dlsym(opc_handler->dl_handle, "opc_recv_amsg");
    if (opc_handler->opc_recv_amsg == NULL)
    {
        perror("dlsym");
        printf("opc_recv_amsg: %s\n", dlerror());
        dlclose(opc_handler->dl_handle);
        return EXIT_FAILURE;
    }

    opc_handler->opc_recv_uri = dlsym(opc_handler->dl_handle, "opc_recv_uri");
    if (opc_handler->opc_recv_uri == NULL)
    {
        perror("dlsym");
        printf("opc_recv_uri: %s\n", dlerror());
        dlclose(opc_handler->dl_handle);
        return EXIT_FAILURE;
    }

    opc_handler->opc_create_status = dlsym(opc_handler->dl_handle, "opc_create_status");
    if (opc_handler->opc_create_status == NULL)
    {
        perror("dlsym");
        printf("opc_create_status: %s\n", dlerror());
        dlclose(opc_handler->dl_handle);
        return EXIT_FAILURE;
    }

    opc_handler->opc_free_status = dlsym(opc_handler->dl_handle, "opc_free_status");
    if (opc_handler->opc_free_status == NULL)
    {
        perror("dlsym");
        printf("opc_free_status: %s\n", dlerror());
        dlclose(opc_handler->dl_handle);
        return EXIT_FAILURE;
    }

    opc_handler->opc_status_get_status = dlsym(opc_handler->dl_handle, "opc_status_get_status");
    if (opc_handler->opc_status_get_status== NULL)
    {
        perror("dlsym");
        printf("opc_status_get_status: %s\n", dlerror());
        dlclose(opc_handler->dl_handle);
        return EXIT_FAILURE;
    }

    opc_handler->opc_status_get_type = dlsym(opc_handler->dl_handle, "opc_status_get_type");
    if (opc_handler->opc_status_get_type == NULL)
    {
        perror("dlsym");
        printf("opc_status_get_type: %s\n", dlerror());
        dlclose(opc_handler->dl_handle);
        return EXIT_FAILURE;
    }

    opc_handler->opc_status_get_cmd = dlsym(opc_handler->dl_handle, "opc_status_get_cmd");
    if (opc_handler->opc_status_get_cmd == NULL)
    {
        perror("dlsym");
        printf("opc_status_get_type: %s\n", dlerror());
        dlclose(opc_handler->dl_handle);
        return EXIT_FAILURE;
    }

    opc_handler->opc_status_get_msg_idx = dlsym(opc_handler->dl_handle, "opc_status_get_msg_idx");
    if (opc_handler->opc_status_get_msg_idx == NULL)
    {
        perror("dlsym");
        printf("opc_status_get_msg_idx: %s\n", dlerror());
        dlclose(opc_handler->dl_handle);
        return EXIT_FAILURE;
    }

    opc_handler->opc_status_get_n_data = dlsym(opc_handler->dl_handle, "opc_status_get_n_data");
    if (opc_handler->opc_status_get_n_data == NULL)
    {
        perror("dlsym");
        printf("opc_status_get_n_data: %s\n", dlerror());
        dlclose(opc_handler->dl_handle);
        return EXIT_FAILURE;
    }

    opc_handler->opc_status_get_data = dlsym(opc_handler->dl_handle, "opc_status_get_data");
    if (opc_handler->opc_status_get_data == NULL)
    {
        perror("dlsym");
        printf("opc_status_get_data: %s\n", dlerror());
        dlclose(opc_handler->dl_handle);
        return EXIT_FAILURE;
    }

    opc_handler->opc_set_log_level = dlsym(opc_handler->dl_handle, "opc_set_log_level");
    if (opc_handler->opc_set_log_level == NULL)
    {
        perror("dlsym");
        printf("opc_set_log_level: %s\n", dlerror());
        dlclose(opc_handler->dl_handle);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int set_opc_client(t_opc_handler* opc_handler, int log_level)
{
    int ret;

    opc_handler->opc_set_log_level(log_level);
    if (opc_handler->opc_set_log_level == NULL)
    {
        printf("Failed to set log\n");
        return EXIT_FAILURE;
    }

    opc_handler->opc_client = opc_handler->opc_init();
    if (opc_handler->opc_client == NULL)
    {
        printf("Failed to create opc client\n");
        return EXIT_FAILURE;
    }

    printf("waiting for server connected...\n");
    ret = opc_handler->opc_connect(opc_handler->opc_client);
    if (ret == 0)
    {
        printf("Failed to connect to server\n");
        opc_handler->opc_destroy(opc_handler->opc_client);
        return EXIT_FAILURE;
    }
    printf("server connected\n");

    ret = opc_handler->opc_prepare(opc_handler->opc_client);
    if (ret == 0)
    {
        printf("Failed to prepare client\n");
        opc_handler->opc_destroy(opc_handler->opc_client);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

