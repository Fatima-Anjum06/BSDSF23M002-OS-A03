#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "../include/shell.h"

#define MAX_COMMAND_LEN 1024

void init_shell() {
    printf("Welcome to MyShell v7 (with if-then-else)\n");
}

void print_prompt() {
    printf("myshell> ");
    fflush(stdout);
}

char *read_command() {
    static char command[MAX_COMMAND_LEN];
    if (!fgets(command, sizeof(command), stdin))
        return NULL;
    command[strcspn(command, "\n")] = '\0';
    return command;
}

void shell_loop() {
    char *command;

    while (1) {
        print_prompt();
        command = read_command();

        if (!command) {
            printf("\n");
            break;
        }

        if (strlen(command) == 0)
            continue;

        // Exit command
        if (strcmp(command, "exit") == 0)
            break;

        // ==========================
        // IF-THEN-ELSE-FI BLOCK
        // ==========================
        if (strncmp(command, "if ", 3) == 0) {
            char if_cmd[256];
            strcpy(if_cmd, command + 3);

            char then_block[2048] = "";
            char else_block[2048] = "";
            int in_else = 0;

            while (1) {
                printf("... "); // continuation prompt
                char *line = read_command();
                if (!line) break;

                if (strcmp(line, "then") == 0)
                    continue;
                else if (strcmp(line, "else") == 0) {
                    in_else = 1;
                    continue;
                } else if (strcmp(line, "fi") == 0)
                    break;

                if (in_else)
                    strcat(else_block, line), strcat(else_block, "\n");
                else
                    strcat(then_block, line), strcat(then_block, "\n");
            }

            // Execute the IF condition
            pid_t pid = fork();
            if (pid == 0) {
                execlp("sh", "sh", "-c", if_cmd, NULL);
                perror("if command failed");
                exit(1);
            } else {
                int status;
                waitpid(pid, &status, 0);
                int exit_status = WEXITSTATUS(status);

                if (exit_status == 0 && strlen(then_block) > 0)
                    system(then_block);
                else if (exit_status != 0 && strlen(else_block) > 0)
                    system(else_block);
            }

            continue;
        }

        // ==========================
        // NORMAL COMMAND EXECUTION
        // ==========================
        execute_command(command, 0); // FIXED: added background flag = 0
    }
}
