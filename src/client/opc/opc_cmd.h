// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#ifndef __OPC_CMD_H__
# define __OPC_CMD_H__

# include "./opclient.h"

void    init_cmd_array(char** cmd_array);
char*   command_to_string(Cmd command, char** cmd_array);

#endif /* __OPC_CMD_H__ */
