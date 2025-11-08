#ifndef SHELL_H
#define SHELL_H

#include <sys/types.h>   // <-- Add this line

#define MAX_COMMAND_LEN 1024
#define MAX_TOKENS 128
#define MAX_PIPELINE 32
#define MAX_JOBS 128

/* Core */
void init_shell(void);
void shell_loop(void);
char *read_command(void);

/* Execution */
void execute_command(char *command, int background);

/* Built-ins */
int handle_builtin(char **args);
void print_help(void);

/* Job management (background jobs) */
void init_jobs(void);
void add_job(pid_t pid, const char *cmd);
void remove_job(pid_t pid);
void print_jobs(void);
void reap_background_jobs(void);

#endif /* SHELL_H */

