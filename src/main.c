#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../include/shell.h"

int main() {
    char command[100];

    init_shell();

    while (1) {
        print_prompt();
        read_command(command);

        // remove trailing newline
        command[strcspn(command, "\n")] = '\0';

        if (strlen(command) == 0)
            continue;

        // Exit command
        if (strcmp(command, "exit") == 0) {
            printf("Exiting shell...\n");
            break;
        }

        execute_command(command);
    }

    return 0;
}
