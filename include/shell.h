#ifndef SHELL_H
#define SHELL_H

void init_shell();
void print_prompt();
void read_command(char *command);
void execute_command(char *command);

#endif
