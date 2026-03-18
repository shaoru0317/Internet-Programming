#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_LINE       5001
#define MAX_ARGS       64
#define MAX_PIPE_SLOTS 100

/* Numbered pipe entry */
typedef struct {
    int number;   /* the Nth numbered pipe */
    int fd[2];    /* fd[0]=read, fd[1]=write */
    int active;
} NumberedPipe;

extern NumberedPipe npipes[MAX_PIPE_SLOTS];
extern int current_line;  /* current command line number (for !N) */

/* Parse & execute */
int   run_line(char *line);  /* returns 1 if builtin was handled */
int   builtin_cmd(char **argv, int argc);
void  exec_pipeline(char *line);

/* Numbered pipe helpers */
int   find_npipe(int n);
int   create_npipe(int n);
void  close_npipe(int slot);

/* Environment */
void  builtin_setenv(char **argv, int argc);
void  builtin_printenv(char **argv, int argc);

#endif /* SHELL_H */
