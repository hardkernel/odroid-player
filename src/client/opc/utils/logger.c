#include "logger.h"

static int  log_level = QUIET;

static const char*  level_names[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

void    set_log_level(int level)
{
    log_level = level;
}

void    logger(int level, const char* file, int line, const char* fmt, ...)
{
    struct timespec tm;
    long            second, usec;
    va_list         args;

    if (level < log_level)
    {
        return;
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &tm);
    second = tm.tv_sec;
    usec = tm.tv_nsec/1000LL;

    printf("[%ld.%06ld]: %-5s %s:%d [odroid-player-client]: ", 
                                                        second, usec,
                                                        level_names[level],
                                                        file, line);
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

