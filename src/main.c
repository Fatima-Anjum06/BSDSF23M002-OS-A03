#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "shell.h"

int main(void) {
    char command[MAX_COMMAND_LEN];

    init_shell(); /* also initializes history */

    while (1) {
        print_prompt();
        read_command(command);

        /* If EOF or empty input (read_command sets empty string on EOF), handle it */
        if (command[0] == '\0') {
            /* if EOF, break loop */
            /* We treat empty input as just skip */
            continue;
        }

        /* Check for !n syntax BEFORE adding to history or tokenizing */
        if (command[0] == '!' && command[1] != '\0') {
            /* parse number after ! */
            char *endptr;
            long n = strtol(command + 1, &endptr, 10);
            if (endptr == command + 1 || n <= 0) {
                fprintf(stderr, "Invalid history reference: %s\n", command);
                continue;
            }
            const char *hcmd = get_history_command((int)n);
            if (hcmd == NULL) {
                fprintf(stderr, "No such command in history: %ld\n", n);
                continue;
            }
            /* Replace command buffer contents with the retrieved history command */
            strncpy(command, hcmd, MAX_COMMAND_LEN - 1);
            command[MAX_COMMAND_LEN - 1] = '\0';
            printf("%s\n", command); /* echo the expanded command (common shell behavior) */
        }

        /* Add the (possibly expanded) command to history */
        add_history_entry(command);

        /* Execute the command (execute_command will check builtins) */
        execute_command(command);
    }

    return 0;
}
