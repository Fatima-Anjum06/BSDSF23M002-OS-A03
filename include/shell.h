#ifndef SHELL_H
#define SHELL_H

#define MAX_COMMAND_LEN 1024
#define MAX_TOKENS 64

void init_shell();
void print_prompt();
void read_command(char *command);
void execute_command(char *command);
int handle_builtin(char **args); // added for built-in commands

#endif
