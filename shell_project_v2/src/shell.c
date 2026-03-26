#include "shell.h"

extern char **environ;

/* step 2: parser command string */
command_t *parser(char *commandStr) {
    command_t *cmd = (command_t *)malloc(sizeof(command_t));
    memset(cmd, 0, sizeof(command_t));
    sscanf(commandStr, "%s", cmd->command);
    int x = strlen(cmd->command) + 1;
    if (x <= (int)strlen(commandStr))
        sscanf(commandStr + x, "%[^\n]", cmd->paramater);
    return cmd;
}

/* step 3: builtin commands */
int do_builtin(command_t *cmd) {
    if (strcmp(cmd->command, "quit") == 0 || strcmp(cmd->command, "exit") == 0) {
        free(cmd);
        exit(0);
    }
    if (strcmp(cmd->command, "printenv") == 0) {
        if (strlen(cmd->paramater) > 0) {
            char *val = getenv(cmd->paramater);
            if (val) printf("%s\n", val);
        } else {
            char **ep;
            for (ep = environ; *ep != NULL; ep++)
                printf("%s\n", *ep);
        }
        return 1;
    }
    if (strcmp(cmd->command, "setenv") == 0) {
        char var[256] = {0}, val[256] = {0};
        sscanf(cmd->paramater, "%s %s", var, val);
        if (strlen(var) > 0)
            setenv(var, val, 1);
        return 1;
    }
    return 0;
}

/* npipe helpers */
int find_npipe(int target) {
    int i;
    for (i = 0; i < PIPE_SLOTS; i++) {
        if (npipes[i].used && npipes[i].num == target)
            return i;
    }
    return -1;
}

int create_npipe(int target) {
    int idx = find_npipe(target);
    if (idx != -1) return idx;

    int i;
    for (i = 0; i < PIPE_SLOTS; i++) {
        if (!npipes[i].used) {
            pipe(npipes[i].fd);
            npipes[i].num = target;
            npipes[i].used = 1;
            return i;
        }
    }
    fprintf(stderr, "too many pipes\n");
    return -1;
}

void close_npipe(int idx) {
    if (idx < 0) return;
    close(npipes[idx].fd[READ_END]);
    close(npipes[idx].fd[WRITE_END]);
    npipes[idx].used = 0;
}

/* close all npipe fds in child process */
static void close_all_npipe_fds() {
    int i;
    for (i = 0; i < PIPE_SLOTS; i++) {
        if (npipes[i].used) {
            close(npipes[i].fd[READ_END]);
            close(npipes[i].fd[WRITE_END]);
        }
    }
}

/* check if numbered pipe has data for current line */
static int check_npipe_in() {
    int idx = find_npipe(line_count);
    if (idx != -1) {
        close(npipes[idx].fd[WRITE_END]);
        npipes[idx].fd[WRITE_END] = -1;
        return npipes[idx].fd[READ_END];
    }
    return -1;
}

static void cleanup_npipe_in() {
    int idx = find_npipe(line_count);
    if (idx != -1)
        npipes[idx].used = 0;
}

/* step 4: fork and exec single command (no pipe)
   ref: week3 case fork + execvp */
void do_exec(command_t *cmd) {
    char *argVec[64];
    int argc = 0;
    char buf[5000];

    argVec[argc++] = cmd->command;
    strcpy(buf, cmd->paramater);
    char *tok = strtok(buf, " \t");
    while (tok != NULL && argc < 63) {
        argVec[argc++] = tok;
        tok = strtok(NULL, " \t");
    }
    argVec[argc] = NULL;

    int in_fd = check_npipe_in();

    switch (fork()) {
        case -1:
            perror("fork");
            break;
        case 0: /* child process */
            if (in_fd != -1) {
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            }
            close_all_npipe_fds();
            execvp(argVec[0], argVec);
            fprintf(stderr, "Unknown command: [%s].\n", argVec[0]);
            exit(1);
        default: /* parent */
            if (in_fd != -1) close(in_fd);
            cleanup_npipe_in();
            wait(NULL);
            break;
    }
}

/* step 5: pipe command (e.g. "ls | cat | wc")
   ref: week4 case 3-4, two child process communicate with pipe
   child1 stdout -> pipe -> child2 stdin, chain them together */
void do_pipe(command_t *cmd) {
    /* combine command + paramater, then split by | */
    char fullcmd[MAXLINE + 256];
    if (strlen(cmd->paramater) > 0)
        snprintf(fullcmd, sizeof(fullcmd), "%s %s", cmd->command, cmd->paramater);
    else
        snprintf(fullcmd, sizeof(fullcmd), "%s", cmd->command);

    /* split by | */
    char *subcmds[64];
    int nsub = 0;
    char *tok = strtok(fullcmd, "|");
    while (tok != NULL && nsub < 64) {
        subcmds[nsub++] = tok;
        tok = strtok(NULL, "|");
    }
    if (nsub == 0) return;

    int in_fd = check_npipe_in();
    int prev_fd = in_fd;
    int fd[2];
    int i;

    for (i = 0; i < nsub; i++) {
        /* parse each sub-command into argVec */
        char *argVec[64];
        int ac = 0;
        char *t = strtok(subcmds[i], " \t");
        while (t != NULL && ac < 63) {
            argVec[ac++] = t;
            t = strtok(NULL, " \t");
        }
        argVec[ac] = NULL;
        if (ac == 0) continue;

        /* create pipe between commands, except last one */
        if (i < nsub - 1)
            pipe(fd);

        switch (fork()) {
            case -1:
                perror("fork");
                return;
            case 0: /* child */
                /* input from previous pipe or npipe */
                if (prev_fd != -1) {
                    dup2(prev_fd, STDIN_FILENO);
                    close(prev_fd);
                }
                /* output to next pipe (not for last command) */
                if (i < nsub - 1) {
                    close(fd[READ_END]);
                    dup2(fd[WRITE_END], STDOUT_FILENO);
                    close(fd[WRITE_END]);
                }
                close_all_npipe_fds();
                execvp(argVec[0], argVec);
                fprintf(stderr, "Unknown command: [%s].\n", argVec[0]);
                exit(1);
            default: /* parent */
                if (prev_fd != -1) close(prev_fd);
                if (i < nsub - 1) {
                    close(fd[WRITE_END]);
                    prev_fd = fd[READ_END];
                }
                break;
        }
    }

    cleanup_npipe_in();

    /* wait for all children in this pipeline */
    for (i = 0; i < nsub; i++)
        wait(NULL);
}

/* step 6: number pipe |n or !n
   ref: week4 pipe + fifo concept, redirect stdout to numbered pipe */
void do_npipe(command_t *cmd, int n, int with_err) {
    int target = line_count + n;
    int idx = create_npipe(target);
    if (idx < 0) return;

    /* build argVec, cut paramater before | or ! */
    char *argVec[64];
    int argc = 0;
    char buf[5000];

    argVec[argc++] = cmd->command;
    strcpy(buf, cmd->paramater);
    char *stop = strchr(buf, '|');
    if (!stop) stop = strchr(buf, '!');
    if (stop) *stop = '\0';

    char *tok = strtok(buf, " \t");
    while (tok != NULL && argc < 63) {
        argVec[argc++] = tok;
        tok = strtok(NULL, " \t");
    }
    argVec[argc] = NULL;

    int in_fd = check_npipe_in();

    switch (fork()) {
        case -1:
            perror("fork");
            break;
        case 0: /* child */
            if (in_fd != -1) {
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            }
            /* stdout -> numbered pipe */
            dup2(npipes[idx].fd[WRITE_END], STDOUT_FILENO);
            if (with_err)
                dup2(npipes[idx].fd[WRITE_END], STDERR_FILENO);
            close_all_npipe_fds();
            execvp(argVec[0], argVec);
            fprintf(stderr, "Unknown command: [%s].\n", argVec[0]);
            exit(1);
        default: /* parent */
            if (in_fd != -1) close(in_fd);
            cleanup_npipe_in();
            wait(NULL);
            break;
    }
}
