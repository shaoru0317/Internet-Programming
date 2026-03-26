#include "shell.h"
#include <signal.h>

NPipe pipes_table[PIPE_SLOTS];
int line_count = 1;

// setenv, printenv
extern char **environ;

void my_setenv(char **args, int n)
{
    if (n < 2) {
        fprintf(stderr, "usage: setenv VAR [value]\n");
        return;
    }
    setenv(args[1], (n >= 3) ? args[2] : "", 1);
}

void my_printenv(char **args, int n)
{
    if (n == 1) {
        // print all
        char **e;
        for (e = environ; *e; e++)
            printf("%s\n", *e);
    } else {
        char *v = getenv(args[1]);
        if (v) printf("%s\n", v);
    }
}

int handle_builtin(char **args, int n)
{
    if (n == 0) return 1;
    if (strcmp(args[0], "quit") == 0 || strcmp(args[0], "exit") == 0)
        exit(0);
    if (strcmp(args[0], "setenv") == 0) {
        my_setenv(args, n);
        return 1;
    }
    if (strcmp(args[0], "printenv") == 0) {
        my_printenv(args, n);
        return 1;
    }
    return 0;
}

// npipe helpers
int find_pipe(int n)
{
    int i;
    for (i = 0; i < PIPE_SLOTS; i++) {
        if (pipes_table[i].used && pipes_table[i].num == n)
            return i;
    }
    return -1;
}

int make_pipe(int n)
{
    int idx = find_pipe(n);
    if (idx != -1) return idx;

    int i;
    for (i = 0; i < PIPE_SLOTS; i++) {
        if (!pipes_table[i].used) {
            pipe(pipes_table[i].fd);
            pipes_table[i].num = n;
            pipes_table[i].used = 1;
            return i;
        }
    }
    fprintf(stderr, "too many pipes\n");
    return -1;
}

void free_pipe(int idx)
{
    if (idx < 0 || !pipes_table[idx].used) return;
    close(pipes_table[idx].fd[0]);
    close(pipes_table[idx].fd[1]);
    pipes_table[idx].used = 0;
}

int main()
{
    char buf[MAXLINE];
    memset(pipes_table, 0, sizeof(pipes_table));
    setenv("PATH", "bin:.", 1);

    // ignore child signal to prevent zombie
    signal(SIGCHLD, SIG_IGN);

    while (1) {
        printf("%% ");
        fflush(stdout);

        if (fgets(buf, MAXLINE, stdin) == NULL)
            break;

        // remove newline
        int len = strlen(buf);
        if (len > 0 && buf[len-1] == '\n')
            buf[len-1] = '\0';

        if (buf[0] == '\0')
            continue;

        // check if builtin first (skip line count)
        char *p = buf;
        while (*p == ' ') p++;

        if (strncmp(p, "setenv", 6) == 0 || strncmp(p, "printenv", 8) == 0
            || strncmp(p, "quit", 4) == 0 || strncmp(p, "exit", 4) == 0) {
            do_cmd(buf);
        } else {
            do_cmd(buf);
            line_count++;
        }
    }

    return 0;
}
