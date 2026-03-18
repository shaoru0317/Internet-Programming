#include "shell.h"

extern char **environ;

void builtin_setenv(char **argv, int argc)
{
    if (argc < 2) {
        fprintf(stderr, "usage: setenv VARIABLE [value]\n");
        return;
    }
    const char *name  = argv[1];
    const char *value = (argc >= 3) ? argv[2] : "";
    if (setenv(name, value, 1) < 0)
        perror("setenv");
}

void builtin_printenv(char **argv, int argc)
{
    if (argc == 1) {
        /* print all */
        for (char **e = environ; *e; e++)
            printf("%s\n", *e);
    } else {
        /* print specific variable */
        const char *val = getenv(argv[1]);
        if (val)
            printf("%s\n", val);
        /* silently succeed if not found (matches typical shell behaviour) */
    }
}

/* Returns 1 if handled as builtin, 0 otherwise */
int builtin_cmd(char **argv, int argc)
{
    if (argc == 0) return 1;

    if (strcmp(argv[0], "exit") == 0 || strcmp(argv[0], "quit") == 0) {
        exit(0);
    }
    if (strcmp(argv[0], "setenv") == 0) {
        builtin_setenv(argv, argc);
        return 1;
    }
    if (strcmp(argv[0], "printenv") == 0) {
        builtin_printenv(argv, argc);
        return 1;
    }
    return 0;
}
