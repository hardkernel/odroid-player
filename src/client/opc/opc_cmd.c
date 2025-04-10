// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#include "opc_cmd.h"

void    init_cmd_array(char** cmd_array)
{
    memset(cmd_array, 0, sizeof(char*) * CMD__CMD_COUNT);

    cmd_array[CMD__NONE] = "none";
    cmd_array[CMD__PLAY] = "play";
    cmd_array[CMD__STOP] = "stop";
    cmd_array[CMD__PAUSE] = "pause";
    cmd_array[CMD__NEXT] = "next";
    cmd_array[CMD__PREV] = "prev";
    cmd_array[CMD__QUIT] = "quit";
    cmd_array[CMD__SEEK] = "seek";
    cmd_array[CMD__TIME] = "time";
    cmd_array[CMD__URI] = "uri";
    cmd_array[CMD__ACK] = "ack";
}

char*   command_to_string(Cmd command, char** cmd_array)
{
    return strdup(cmd_array[command]);
}

