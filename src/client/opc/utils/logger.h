// SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
// SPDX-License-Identifier: Apache-2.0

#ifndef __LOGGER_H__
# define __LOGGER_H__

# include <stdio.h>
# include <stdarg.h>
# include <time.h>

typedef enum
{
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL,
    QUIET
} e_log_level;

# define LOG_TRACE(...) logger(TRACE, __func__, __LINE__, __VA_ARGS__)
# define LOG_DEBUG(...) logger(DEBUG, __func__, __LINE__, __VA_ARGS__)
# define LOG_INFO(...)  logger(INFO,  __func__, __LINE__, __VA_ARGS__)
# define LOG_WARN(...)  logger(WARN,  __func__, __LINE__, __VA_ARGS__)
# define LOG_ERROR(...) logger(ERROR, __func__, __LINE__, __VA_ARGS__)
# define LOG_FATAL(...) logger(FATAL, __func__, __LINE__, __VA_ARGS__)

void    set_log_level(int level);
void    logger(int level, const char* file, int line, const char* fmt, ...);

#endif /* __LOGGER_H__ */

