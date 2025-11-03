/* src/shell.c -- Feature 4: Readline integration with history and tab completion */

#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>

/* Tab-completion helpers */
static char **myshell_completion(const char *text, int start, int end);
static char *command_generator(const char *text, int state);

/* Built-ins for tab completion */
static const char *builtins[] = {"cd", "help", "exit", "jobs", "history", NULL};

/* Initialize shell */
void init_shell(void) {
    printf("Welcome to MyShell (with Readline support)!\n");
    printf("Type 'help' to see available commands.\n\n");
    using_history();   // initialize readline history subsystem
}

/* Read user command using Readline */
void read_command(char *command) {
    char *input = readline("myshell> ");
    if (!input) {
        printf("\n");
        exit(0);  // Ctrl+D pressed
    }

    if (strlen(input) > 0)
        add_history(input);   // builtin Readline function

    strcpy(command, input);
    free(input);
}

/* Tokenize command string */
char **tokenize(char *line) {
    static char *args[MAX_TOKENS];
    char *token;
    int i = 0;

    token = strtok(line, " \t\r\n");
    while (token != NULL && i < MAX_TOKENS - 1) {
        args[i++] = token;
        token = strtok(NULL, " \t\r\n");
    }
    args[i] = NULL;
    return args;
}

/* Shell main loop */
void shell_loop(void) {
    char command[MAX_COMMAND_LEN];
    char **args;

    rl_attempted_completion_function = myshell_completion;

    while (1) {
        read_command(command);

        if (strlen(command) == 0)
            continue;

        args = tokenize(command);
        if (args[0] == NULL)
            continue;

        if (!handle_builtin(args))
            execute_command(command);
    }
}

/* ----------------------- Tab completion ----------------------- */

static char **myshell_completion(const char *text, int start, int end) {
    (void)end;
    if (start == 0)
        return rl_completion_matches(text, command_generator);
    else
        return rl_completion_matches(text, rl_filename_completion_function);
}

static char *command_generator(const char *text, int state) {
    static int list_index, len;
    const char *name;

    if (!state) {
        list_index = 0;
        len = strlen(text);
    }

    while ((name = builtins[list_index++])) {
        if (strncmp(name, text, len) == 0)
            return strdup(name);
    }
    return NULL;
}

