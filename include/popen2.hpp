/**
 * Taken from https://github.com/vi/syscall_limiter/blob/master/writelimiter/popen2.h
 * MIT License
 * 
 */

#pragma once

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

struct popen2 {
    pid_t child_pid;
    int   from_child, to_child;
};

int popen2(const char *cmdline, struct popen2 *childinfo);

