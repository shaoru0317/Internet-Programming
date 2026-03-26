#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#define MAXLINE 5001
#define PIPE_SLOTS 150

#define READ_END 0
#define WRITE_END 1

typedef struct commandType {
    char command[256];
    char paramater[5000];
} command_t;

typedef struct npipeType {
    int num;
    int fd[2];
    int used;
} npipe_t;

extern npipe_t npipes[PIPE_SLOTS];
extern int line_count;

command_t *parser(char *commandStr);
int do_builtin(command_t *cmd);
void do_exec(command_t *cmd);
void do_pipe(command_t *cmd);
void do_npipe(command_t *cmd, int n, int with_err);

int find_npipe(int target);
int create_npipe(int target);
void close_npipe(int idx);

#endif
