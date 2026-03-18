#include "shell.h"

NumberedPipe npipes[MAX_PIPE_SLOTS];
int current_line = 1;

int main(void)
{
    char line[MAX_LINE];

    memset(npipes, 0, sizeof(npipes));
    setenv("PATH", "bin:.", 1);

    while (1) {
        printf("%% ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            /* EOF */
            break;
        }

        /* strip trailing newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[len - 1] = '\0';

        if (line[0] == '\0') {
            /* empty lines don't count toward pipe numbering */
            continue;
        }

        if (!run_line(line))
            current_line++;
    }

    return 0;
}
