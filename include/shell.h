#ifndef SHELL_H
#define SHELL_H

/* limits */
#define MAX_COMMAND_LEN 1024
#define MAX_TOKENS 128
#define MAX_PIPELINE 32

/* core */
void init_shell(void);
void shell_loop(void);
char *read_command(void);

/* execution (accepts the whole input line) */
void execute_command(char *command);

/* built-ins */
int handle_builtin(char **args);
void print_help(void);

#endif /* SHELL_H */
