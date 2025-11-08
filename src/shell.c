/* src/shell.c */
#include "../include/shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <ctype.h>

/* Initialize shell and job system */
void init_shell(void) {
    printf("\n*** Welcome to MyShell (multitasking enabled) ***\n");
    printf("Type 'help' to see built-in commands.\n\n");
    using_history();
    init_jobs();
}

/* Read a line using readline, trim and return allocated string (caller frees) */
char *read_command(void) {
    char *line = readline("myshell> ");
    if (line == NULL) return NULL; /* EOF */

    /* trim leading/trailing whitespace */
    size_t s = 0;
    while (line[s] && isspace((unsigned char)line[s])) s++;
    size_t e = strlen(line);
    while (e > s && isspace((unsigned char)line[e-1])) e--;

    if (s == e) {
        free(line);
        return strdup(""); /* empty */
    }

    size_t len = e - s;
    char *out = malloc(len + 1);
    if (!out) { free(line); return NULL; }
    memcpy(out, line + s, len);
    out[len] = '\0';

    /* add non-empty to history */
    if (len > 0) add_history(out);
    free(line);
    return out;
}

/* Helper: split on semicolons into commands (modifies input), returns count */
static int split_chained(char *line, char *cmds[], int maxcmds) {
    int count = 0;
    char *saveptr = NULL;
    char *tok = strtok_r(line, ";", &saveptr);
    while (tok && count < maxcmds) {
        /* trim leading/trailing spaces */
        while (*tok && isspace((unsigned char)*tok)) tok++;
        char *end = tok + strlen(tok) - 1;
        while (end >= tok && isspace((unsigned char)*end)) { *end = '\0'; end--; }
        if (*tok != '\0') cmds[count++] = tok;
        tok = strtok_r(NULL, ";", &saveptr);
    }
    return count;
}

/* Main loop: splits by ';', handle background '&', reaps finished jobs */
void shell_loop(void) {
    char *line;

    while (1) {
        /* Reap any finished background jobs before showing prompt */
        reap_background_jobs();

        line = read_command();
        if (line == NULL) { printf("\n"); break; } /* EOF */

        if (line[0] == '\0') { free(line); continue; }

        /* Split by semicolons */
        char *commands[64];
        int n = split_chained(line, commands, 64);

        for (int i = 0; i < n; ++i) {
            char *cmd = commands[i];

            /* Check for trailing '&' (background) */
            int bg = 0;
            size_t L = strlen(cmd);
            if (L > 0) {
                /* skip trailing spaces */
                size_t j = L;
                while (j > 0 && isspace((unsigned char)cmd[j-1])) j--;
                if (j > 0 && cmd[j-1] == '&') {
                    bg = 1;
                    /* remove the '&' */
                    cmd[j-1] = '\0';
                    /* trim trailing spaces again */
                    size_t k = j-1;
                    while (k > 0 && isspace((unsigned char)cmd[k-1])) { cmd[k-1] = '\0'; k--; }
                }
            }

            /* Check for special 'exit' quick path */
            if (strcmp(cmd, "exit") == 0) {
                free(line);
                return;
            }

            /* Pass the full command string to executor with bg flag */
            execute_command(cmd, bg);
        }

        free(line);
    }
}
