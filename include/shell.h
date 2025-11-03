#ifndef SHELL_H
#define SHELL_H

#include <stddef.h>

#define MAX_COMMAND_LEN 1024
#define MAX_TOKENS 64
#define HISTORY_SIZE 20

/* core shell operations */
void init_shell();
void print_prompt();
void read_command(char *command);
void execute_command(char *command);

/* history API */
void init_history(void);
void add_history_entry(const char *cmd);
void print_history(void);
const char *get_history_command(int seq_num);
void cleanup_history(void);   // ✅ add this line

int handle_builtin(char **args);

#endif /* SHELL_H */
