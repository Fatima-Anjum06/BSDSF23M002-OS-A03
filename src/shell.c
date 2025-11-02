#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include "../include/shell.h"

void init_shell() {
    printf("Welcome to the Basic Shell!\n");
    printf("Type 'exit' to quit.\n\n");
}

void print_prompt() {
    printf("myShell> ");
    fflush(stdout);
}

void read_command(char *command) {
    fgets(command, 100, stdin);
}

void execute_command(char *command) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    } 
    else if (pid == 0) {
        // child process
        char *args[10];
        int i = 0;

        char *token = strtok(command, " ");
        while (token != NULL) {
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;

        if (execvp(args[0], args) == -1) {
            perror("Command execution failed");
        }
        exit(EXIT_FAILURE);
    } 
    else {
        // parent process waits
        wait(NULL);
    }
}
