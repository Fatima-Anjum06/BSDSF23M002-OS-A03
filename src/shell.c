/* src/shell.c */
#include "../include/shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>

void init_shell(void) {
    printf("\n*** Welcome to MyShell (with Readline) ***\n");
    printf("Type 'help' to see built-in commands.\n\n");
    using_history();
}

char *read_command(void) {
    char *line = readline("myshell> ");
    if (line == NULL) {
        /* EOF (Ctrl+D) */
        return NULL;
    }
    /* Trim leading/trailing whitespace */
    size_t start = 0;
    while (line[start] && (line[start] == ' ' || line[start] == '\t')) start++;
    size_t end = strlen(line);
    while (end > start && (line[end-1] == ' ' || line[end-1] == '\t')) end--;
    if (start >= end) {
        free(line);
        return strdup(""); /* empty string */
    }
    size_t len = end - start;
    char *out = malloc(len + 1);
    if (!out) { free(line); return NULL; }
    memcpy(out, line + start, len);
    out[len] = '\0';

    /* add non-empty to readline history */
    if (len > 0) add_history(out);
    free(line);
    return out;
}

void shell_loop(void) {
    char *line;

    while (1) {
        line = read_command();
        if (line == NULL) { /* EOF */
            printf("\n");
            break;
        }
        if (line[0] == '\0') {
            free(line);
            continue;
        }

        /* handle built-in 'exit' quickly (shortcut) */
        if (strcmp(line, "exit") == 0) {
            free(line);
            break;
        }

        execute_command(line);

        free(line);
    }
}

