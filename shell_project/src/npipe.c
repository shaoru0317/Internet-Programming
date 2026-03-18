#include "shell.h"

/* Find slot index for numbered pipe N, return -1 if not found */
int find_npipe(int n)
{
    for (int i = 0; i < MAX_PIPE_SLOTS; i++) {
        if (npipes[i].active && npipes[i].number == n)
            return i;
    }
    return -1;
}

/* Create (or reuse) numbered pipe for line number N */
int create_npipe(int n)
{
    int slot = find_npipe(n);
    if (slot != -1)
        return slot;

    /* find a free slot */
    for (int i = 0; i < MAX_PIPE_SLOTS; i++) {
        if (!npipes[i].active) {
            if (pipe(npipes[i].fd) < 0) {
                perror("pipe");
                return -1;
            }
            npipes[i].number = n;
            npipes[i].active = 1;
            return i;
        }
    }
    fprintf(stderr, "shell: too many numbered pipes\n");
    return -1;
}

/* Close and free numbered pipe slot */
void close_npipe(int slot)
{
    if (slot < 0 || slot >= MAX_PIPE_SLOTS) return;
    if (!npipes[slot].active) return;

    close(npipes[slot].fd[0]);
    close(npipes[slot].fd[1]);
    npipes[slot].active = 0;
}
