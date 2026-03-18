#include "shell.h"

/* -----------------------------------------------------------------------
 * Token types for pipe operators:
 *   "|"   -> ordinary pipe
 *   "|N"  -> numbered pipe: stdout of this cmd -> pipe for line (current+N)
 *   "!N"  -> numbered pipe with stderr: stdout+stderr -> pipe for line (current+N)
 * ----------------------------------------------------------------------- */

typedef enum {
    PIPE_NONE,    /* last command in pipeline */
    PIPE_PLAIN,   /* | */
    PIPE_NUM,     /* |N */
    PIPE_NUM_ERR  /* !N */
} PipeType;

typedef struct {
    char    *argv[MAX_ARGS];
    int      argc;
    PipeType pipe_out;   /* what comes after this command */
    int      pipe_num;   /* N for |N or !N */
} Cmd;

/* -----------------------------------------------------------------------
 * Split a line into Cmd structs separated by pipe operators.
 * Modifies line in-place (strtok-style).
 * ----------------------------------------------------------------------- */
static int parse_cmds(char *line, Cmd *cmds, int max_cmds)
{
    int ncmds = 0;
    char *p = line;

    while (*p && ncmds < max_cmds) {
        Cmd *cmd = &cmds[ncmds];
        cmd->argc     = 0;
        cmd->pipe_out = PIPE_NONE;
        cmd->pipe_num = 0;

        while (*p) {
            /* skip whitespace */
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '\0') break;

            /* scan one token — stops at space, tab, |, !, or NUL */
            char *tok_start = p;
            while (*p && *p != ' ' && *p != '\t' && *p != '|' && *p != '!')
                p++;

            char stop = *p;
            *p = '\0';  /* null-terminate the token in-place */

            if (p > tok_start && cmd->argc < MAX_ARGS - 1)
                cmd->argv[cmd->argc++] = tok_start;

            if (stop == '\0') break;   /* end of string */

            if (stop == ' ' || stop == '\t') {
                p++;   /* skip past the overwritten space; loop skips rest */
                continue;
            }

            /* stop == '|' or '!' : pipe operator */
            p++;   /* advance past the overwritten pipe char */
            if (stop == '|') {
                if (*p >= '1' && *p <= '9') {
                    cmd->pipe_out = PIPE_NUM;
                    cmd->pipe_num = (int)strtol(p, &p, 10);
                } else {
                    cmd->pipe_out = PIPE_PLAIN;
                }
            } else { /* '!' */
                if (*p >= '1' && *p <= '9') {
                    cmd->pipe_out = PIPE_NUM_ERR;
                    cmd->pipe_num = (int)strtol(p, &p, 10);
                }
                /* '!' not followed by digit: silently ignored */
            }
            break;  /* done collecting tokens for this command */
        }

        cmd->argv[cmd->argc] = NULL;
        if (cmd->argc > 0)
            ncmds++;
        if (cmd->pipe_out == PIPE_NONE)
            break;
    }

    return ncmds;
}

/* -----------------------------------------------------------------------
 * Execute a parsed pipeline.
 * ----------------------------------------------------------------------- */
void exec_pipeline(char *line)
{
    Cmd cmds[MAX_ARGS];
    int ncmds = parse_cmds(line, cmds, MAX_ARGS);
    if (ncmds == 0) return;

    /* If it's a single command with no piping, check builtins first */
    if (ncmds == 1 && cmds[0].pipe_out == PIPE_NONE) {
        if (builtin_cmd(cmds[0].argv, cmds[0].argc))
            return;  /* caller checks return value of run_line */
    }

    /* plain pipe fd between adjacent commands */
    int plain_pipe[2] = {-1, -1};
    int prev_read_fd  = -1;   /* read-end of previous plain pipe */
    int prev_was_plain = 0;

    for (int i = 0; i < ncmds; i++) {
        Cmd *cmd = &cmds[i];

        /* Determine stdin source for this command */
        int in_fd = -1;

        if (i == 0) {
            /* Check if a numbered pipe was written for the current line */
            int slot = find_npipe(current_line);
            if (slot != -1) {
                in_fd = npipes[slot].fd[0];
                /* close write end so we get EOF when all writers done */
                close(npipes[slot].fd[1]);
                npipes[slot].fd[1] = -1;
            }
        } else if (prev_was_plain) {
            in_fd = prev_read_fd;
        }

        /* Determine stdout (and maybe stderr) destination */
        int out_fd       = -1;
        int pipe_stderr  = 0;   /* 1 if stderr should also go to out_fd */
        int next_read_fd = -1;

        if (cmd->pipe_out == PIPE_PLAIN) {
            if (pipe(plain_pipe) < 0) { perror("pipe"); exit(1); }
            out_fd       = plain_pipe[1];
            next_read_fd = plain_pipe[0];
        } else if (cmd->pipe_out == PIPE_NUM || cmd->pipe_out == PIPE_NUM_ERR) {
            int target_line = current_line + cmd->pipe_num;
            int slot = create_npipe(target_line);
            if (slot < 0) exit(1);
            out_fd = npipes[slot].fd[1];
            if (cmd->pipe_out == PIPE_NUM_ERR)
                pipe_stderr = 1;
        }

        /* Fork */
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); exit(1); }

        if (pid == 0) {
            /* Child */
            if (in_fd != -1) {
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            }
            if (out_fd != -1) {
                dup2(out_fd, STDOUT_FILENO);
                if (pipe_stderr)
                    dup2(out_fd, STDERR_FILENO);
                close(out_fd);
            }

            /* Close all numbered pipe fds we inherited */
            for (int s = 0; s < MAX_PIPE_SLOTS; s++) {
                if (npipes[s].active) {
                    close(npipes[s].fd[0]);
                    close(npipes[s].fd[1]);
                }
            }
            /* Close plain pipe ends we may still hold */
            if (next_read_fd != -1) close(next_read_fd);

            execvp(cmd->argv[0], cmd->argv);
            fprintf(stderr, "Unknown command: %s.\n", cmd->argv[0]);
            exit(127);
        }

        /* Parent: close ends we handed to the child */
        if (in_fd != -1) close(in_fd);
        if (out_fd != -1) close(out_fd);
        /* err_fd is same fd as out_fd for numbered pipe – already closed */

        /* If we consumed a numbered pipe's read end, deactivate the slot */
        if (i == 0) {
            int slot = find_npipe(current_line);
            if (slot != -1 && npipes[slot].fd[1] == -1) {
                /* write end was closed above; now read end consumed by child */
                npipes[slot].active = 0;
            }
        }

        prev_read_fd  = next_read_fd;
        prev_was_plain = (cmd->pipe_out == PIPE_PLAIN);
    }

    /* Wait for all children */
    while (wait(NULL) > 0)
        ;

    /* Clean up any numbered pipe for current_line that was not consumed */
    int slot = find_npipe(current_line);
    if (slot != -1) {
        close_npipe(slot);
    }
}

/* -----------------------------------------------------------------------
 * Entry point called from main for each input line.
 * ----------------------------------------------------------------------- */
int run_line(char *line)
{
    /* skip leading whitespace */
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '\0') return 1;  /* whitespace-only: treat like empty, don't count */

    /* Quick check: is the first word a builtin? */
    const char *first = line;
    const char *end = first;
    while (*end && *end != ' ' && *end != '\t') end++;
    size_t len = (size_t)(end - first);

    if ((len == 6 && strncmp(first, "setenv", 6) == 0) ||
        (len == 8 && strncmp(first, "printenv", 8) == 0) ||
        (len == 4 && strncmp(first, "quit", 4) == 0) ||
        (len == 4 && strncmp(first, "exit", 4) == 0)) {
        exec_pipeline(line);
        return 1;  /* builtin: don't count toward line numbering */
    }

    exec_pipeline(line);
    return 0;  /* external command: count toward line numbering */
}
