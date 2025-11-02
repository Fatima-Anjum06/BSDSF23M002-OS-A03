#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "shell.h"

// Built-in command handler
int handle_builtin(char **args) {
    if (args[0] == NULL)
        return 1;

    if (strcmp(args[0], "exit") == 0) {
        printf("Exiting shell...\n");
        exit(0);
    } 
    else if (strcmp(args[0], "cd") == 0) {
        if (args[1] == NULL)
            fprintf(stderr, "cd: expected argument\n");
        else if (chdir(args[1]) != 0)
            perror("cd");
        return 1;
    } 
    else if (strcmp(args[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("  cd <dir>  - Change directory\n");
        printf("  help      - Display this help message\n");
        printf("  jobs      - Placeholder for job control\n");
        printf("  exit      - Exit the shell\n");
        return 1;
    } 
    else if (strcmp(args[0], "jobs") == 0) {
        printf("Job control not yet implemented.\n");
        return 1;
    }

    return 0; // Not built-in
}

// Execute commands
void execute_command(char *command) {
    char *args[MAX_TOKENS];
    char *token = strtok(command, " \t\r\n");
    int i = 0;

    while (token != NULL && i < MAX_TOKENS - 1) {
        args[i++] = token;
        token = strtok(NULL, " \t\r\n");
    }
    args[i] = NULL;

    if (args[0] == NULL)
        return;

    // Check built-ins before fork
    if (handle_builtin(args))
        return;

    pid_t pid = fork();

    if (pid == 0) {
        // Child
        if (execvp(args[0], args) == -1)
            perror("myshell");
        exit(EXIT_FAILURE);
    } 
    else if (pid > 0) {
        // Parent waits
        int status;
        waitpid(pid, &status, 0);
    } 
    else {
        perror("fork");
    }
}
