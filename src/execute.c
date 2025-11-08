/* src/execute.c -- Feature 5/6/7/8 combined:
   - I/O redirection (<, >)
   - Pipes (|)
   - Background jobs (&)
   - if-then-else handled in shell.c
   - Shell variables: assignment VARNAME=value, expansion $VARNAME, 'set' builtin
*/

#define _GNU_SOURCE
#include "../include/shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>

/* ---------- Variable storage (linked list) ---------- */

typedef struct var_s {
    char *name;
    char *value;
    struct var_s *next;
} var_t;

static var_t *var_list = NULL;

/* set or update variable */
static void set_variable(const char *name, const char *value) {
    if (!name) return;
    var_t *cur = var_list;
    while (cur) {
        if (strcmp(cur->name, name) == 0) {
            /* update */
            free(cur->value);
            cur->value = strdup(value ? value : "");
            return;
        }
        cur = cur->next;
    }
    /* not found -> add */
    var_t *node = malloc(sizeof(var_t));
    node->name = strdup(name);
    node->value = strdup(value ? value : "");
    node->next = var_list;
    var_list = node;
}

/* get variable value (returns NULL if not found) */
static const char *get_variable(const char *name) {
    if (!name) return NULL;
    var_t *cur = var_list;
    while (cur) {
        if (strcmp(cur->name, name) == 0) return cur->value;
        cur = cur->next;
    }
    return NULL;
}

/* print all variables (for 'set' builtin) */
static void print_variables(void) {
    var_t *cur = var_list;
    if (!cur) {
        printf("No variables defined.\n");
        return;
    }
    while (cur) {
        printf("%s=%s\n", cur->name, cur->value);
        cur = cur->next;
    }
}

/* free all variables (on exit if desired) */
static void free_all_variables(void) {
    var_t *cur = var_list;
    while (cur) {
        var_t *n = cur->next;
        free(cur->name);
        free(cur->value);
        free(cur);
        cur = n;
    }
    var_list = NULL;
}

/* trim whitespace in-place */
static void trim(char *s) {
    if (!s) return;
    /* left */
    while (*s && isspace((unsigned char)*s)) memmove(s, s+1, strlen(s));
    /* right */
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len-1])) { s[len-1] = '\0'; len--; }
}

/* ---------- existing job-management functions (assumed previously present) ---------- */

/* Keep simple jobs implementation — if you already have these in execute.c from Feature-6,
   ensure names/signatures match. If you already have a working version, keep it.
   Below is a minimal placeholder job list; if you have fuller implementation, use that. */

#ifndef MAX_JOBS
#define MAX_JOBS 128
#endif

typedef struct {
    pid_t pid;
    char *cmd;
    int active;
} job_t;

static job_t jobs[MAX_JOBS];

void init_jobs(void) {
    for (int i = 0; i < MAX_JOBS; ++i) { jobs[i].pid = 0; jobs[i].cmd = NULL; jobs[i].active = 0; }
}

void add_job(pid_t pid, const char *cmd) {
    for (int i = 0; i < MAX_JOBS; ++i) {
        if (!jobs[i].active) {
            jobs[i].pid = pid;
            jobs[i].cmd = strdup(cmd ? cmd : "");
            jobs[i].active = 1;
            printf("[bg] started PID %d: %s\n", (int)pid, jobs[i].cmd);
            return;
        }
    }
    fprintf(stderr, "Job list full; cannot track PID %d\n", (int)pid);
}

void remove_job(pid_t pid) {
    for (int i = 0; i < MAX_JOBS; ++i) {
        if (jobs[i].active && jobs[i].pid == pid) {
            jobs[i].active = 0;
            free(jobs[i].cmd);
            jobs[i].cmd = NULL;
            jobs[i].pid = 0;
            return;
        }
    }
}

void print_jobs(void) {
    int count = 0;
    for (int i = 0; i < MAX_JOBS; ++i) {
        if (jobs[i].active) {
            printf("[%d] PID %d  %s\n", i+1, (int)jobs[i].pid, jobs[i].cmd);
            count++;
        }
    }
    if (count == 0) printf("No background jobs.\n");
}

void reap_background_jobs(void) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < MAX_JOBS; ++i) {
            if (jobs[i].active && jobs[i].pid == pid) {
                if (WIFEXITED(status)) {
                    printf("[bg] PID %d finished with exit status %d: %s\n", (int)pid, WEXITSTATUS(status), jobs[i].cmd);
                } else if (WIFSIGNALED(status)) {
                    printf("[bg] PID %d terminated by signal %d: %s\n", (int)pid, WTERMSIG(status), jobs[i].cmd);
                } else {
                    printf("[bg] PID %d changed state: %s\n", (int)pid, jobs[i].cmd);
                }
                remove_job(pid);
                break;
            }
        }
    }
}

/* ---------- Parsing helpers ---------- */

static int split_pipeline(char *line, char *segments[], int maxseg) {
    int n = 0;
    char *saveptr = NULL;
    char *tok = strtok_r(line, "|", &saveptr);
    while (tok && n < maxseg) {
        trim(tok);
        segments[n++] = tok;
        tok = strtok_r(NULL, "|", &saveptr);
    }
    return n;
}

/* parse a segment and perform variable expansion on tokens that begin with $.
   Returns argc or -1 on syntax error.
   argv[] will contain pointers either to substrings inside seg or newly allocated strings
   (when expansion happened). expanded[] flags indicate which argv entries must be freed by caller.
*/
static int parse_segment_and_expand(char *seg, char *argv[], int expanded[]) {
    for (int i = 0; i < MAX_TOKENS; ++i) { argv[i] = NULL; if (expanded) expanded[i] = 0; }
    char *saveptr = NULL;
    char *tok = strtok_r(seg, " \t", &saveptr);
    int argc = 0;

    while (tok != NULL && argc < MAX_TOKENS - 1) {
        if (strcmp(tok, "<") == 0 || strcmp(tok, ">") == 0) {
            /* caller will handle redirections in a higher-level function; here we simply pass tokens through */
            argv[argc++] = tok;
            tok = strtok_r(NULL, " \t", &saveptr);
            if (tok == NULL) return -1; /* syntax error: expected filename after redir token */
            argv[argc++] = tok;
            tok = strtok_r(NULL, " \t", &saveptr);
            continue;
        }

        /* Variable expansion for tokens that begin with '$' */
        if (tok[0] == '$' && tok[1] != '\0') {
            const char *val = get_variable(tok + 1);
            char *replacement = NULL;
            if (val) replacement = strdup(val);
            else replacement = strdup(""); /* undefined -> empty string */
            argv[argc++] = replacement;
            if (expanded) expanded[argc-1] = 1;
        } else {
            argv[argc++] = tok;
            if (expanded) expanded[argc-1] = 0;
        }

        tok = strtok_r(NULL, " \t", &saveptr);
    }

    argv[argc] = NULL;
    return argc;
}

/* parse a segment to extract argv[], infile and outfile (using pre-expanded tokens).
   This function treats < and > specially.
   The argv array returned will be compact (no redirection tokens).
*/
static int build_exec_argv_from_tokens(char *tokens[], int ntokens, char *argv[]) {
    int argc = 0;
    for (int i = 0; i < ntokens; ++i) {
        if (tokens[i] == NULL) continue;
        if (strcmp(tokens[i], "<") == 0 || strcmp(tokens[i], ">") == 0) {
            /* skip operator and its operand here (handled separately by caller) */
            i++; /* skip filename */
            continue;
        } else {
            argv[argc++] = tokens[i];
        }
    }
    argv[argc] = NULL;
    return argc;
}

/* find infile/outfile in token list (returns filename pointer or NULL) */
static char *find_redir_filename(char *tokens[], int ntokens, const char *op) {
    for (int i = 0; i < ntokens; ++i) {
        if (tokens[i] && strcmp(tokens[i], op) == 0) {
            if (i+1 < ntokens) return tokens[i+1];
            else return NULL;
        }
    }
    return NULL;
}

/* ---------- Built-in handler (includes 'set' and 'cd', 'help', 'jobs') ---------- */
int handle_builtin(char **args) {
    if (!args || !args[0]) return 0;

    if (strcmp(args[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("  cd <dir>    Change directory\n");
        printf("  help        Show this message\n");
        printf("  jobs        List background jobs\n");
        printf("  set         Show shell variables\n");
        printf("  exit        Exit the shell\n");
        return 1;
    }
    if (strcmp(args[0], "cd") == 0) {
        if (!args[1]) fprintf(stderr, "cd: expected argument\n");
        else if (chdir(args[1]) != 0) perror("cd");
        return 1;
    }
    if (strcmp(args[0], "jobs") == 0) {
        print_jobs();
        return 1;
    }
    if (strcmp(args[0], "set") == 0) {
        print_variables();
        return 1;
    }
    if (strcmp(args[0], "exit") == 0) {
        /* handled elsewhere usually, but treat as builtin */
        return 1;
    }

    return 0; /* not built-in */
}

/* ---------- Main execution routine (with variable assignment detection) ---------- */

void execute_command(char *command, int background) {
    if (!command) return;

    trim(command);
    if (command[0] == '\0') return;

    /* 1) Detect pure assignment: form VARNAME=value with no spaces before '=' */
    char *eq = strchr(command, '=');
    if (eq != NULL) {
        /* check that there is no whitespace in the left side (before '=') */
        int pos = eq - command;
        int bad = 0;
        if (pos == 0) bad = 1;
        for (int i = 0; i < pos; ++i) if (isspace((unsigned char)command[i])) bad = 1;
        /* also ensure variable name characters are valid */
        if (!bad) {
            /* extract name and value */
            char *name = strndup(command, pos);
            trim(name);
            int valid = 1;
            for (char *p = name; *p; ++p) {
                if (!(isalnum((unsigned char)*p) || *p == '_')) { valid = 0; break; }
            }
            if (valid) {
                char *value = strdup(eq + 1);
                /* remove surrounding quotes if present "..." or '...' */
                if ((value[0] == '"' && value[strlen(value)-1] == '"') ||
                    (value[0] == '\'' && value[strlen(value)-1] == '\'')) {
                    value[strlen(value)-1] = '\0';
                    memmove(value, value+1, strlen(value));
                }
                set_variable(name, value);
                free(name);
                free(value);
                return; /* assignment handled, no fork */
            }
            free(name);
        }
    }

    /* 2) Not a pure assignment: proceed to parse pipelines, redirections, expansions */

    /* make a mutable copy we will strtok() */
    char *line = strdup(command);
    if (!line) { perror("strdup"); return; }

    /* split into pipeline segments */
    char *segments[MAX_PIPELINE];
    int nseg = split_pipeline(line, segments, MAX_PIPELINE);
    if (nseg <= 0) { free(line); return; }

    /* For each segment, create token list with expansions */
    char *tokens[MAX_PIPELINE][MAX_TOKENS];
    int ntokens[MAX_PIPELINE];
    int expanded_flags[MAX_PIPELINE][MAX_TOKENS]; /* mark allocated expansions */

    for (int i = 0; i < nseg; ++i) {
        /* first zero arrays */
        for (int j = 0; j < MAX_TOKENS; ++j) { tokens[i][j] = NULL; expanded_flags[i][j] = 0; }
        /* parse and expand */
        /* We can't use previous parse_segment_and_expand that returned argv directly,
           because we need separate handling of redirection tokens vs exec argv.
           We'll parse tokens and keep them in tokens[i][] */
        char *saveptr = NULL;
        int idx = 0;
        char *tok = strtok_r(segments[i], " \t", &saveptr);
        while (tok && idx < MAX_TOKENS-1) {
            if (tok[0] == '$' && tok[1] != '\0') {
                const char *val = get_variable(tok + 1);
                char *replacement = NULL;
                if (val) replacement = strdup(val);
                else replacement = strdup("");
                tokens[i][idx++] = replacement;
                expanded_flags[i][idx-1] = 1;
            } else {
                tokens[i][idx++] = tok;
                expanded_flags[i][idx-1] = 0;
            }
            tok = strtok_r(NULL, " \t", &saveptr);
        }
        tokens[i][idx] = NULL;
        ntokens[i] = idx;
    }

    /* If single segment and it's a builtin => handle in parent (no fork) */
    if (nseg == 1) {
        /* build a compact argv excluding redir tokens */
        char *argv_single[MAX_TOKENS];
        int argc_single = build_exec_argv_from_tokens(tokens[0], ntokens[0], argv_single);
        if (argc_single > 0 && handle_builtin(argv_single)) {
            /* free any allocated expansions for this segment */
            for (int k = 0; k < ntokens[0]; ++k) if (expanded_flags[0][k]) free(tokens[0][k]);
            free(line);
            return;
        }
    }

    /* Create pipes if needed */
    int pipes[MAX_PIPELINE - 1][2];
    for (int i = 0; i < nseg - 1; ++i) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            for (int j = 0; j < i; ++j) { close(pipes[j][0]); close(pipes[j][1]); }
            /* free expansions */
            for (int a = 0; a < nseg; ++a) for (int b = 0; b < ntokens[a]; ++b) if (expanded_flags[a][b]) free(tokens[a][b]);
            free(line);
            return;
        }
    }

    pid_t pids[MAX_PIPELINE];
    for (int i = 0; i < nseg; ++i) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            for (int j = 0; j < nseg - 1; ++j) { close(pipes[j][0]); close(pipes[j][1]); }
            /* free expansions */
            for (int a = 0; a < nseg; ++a) for (int b = 0; b < ntokens[a]; ++b) if (expanded_flags[a][b]) free(tokens[a][b]);
            free(line);
            return;
        }

        if (pid == 0) {
            /* Child: set up pipes */
            if (i > 0) {
                if (dup2(pipes[i-1][0], STDIN_FILENO) < 0) { perror("dup2"); _exit(1); }
            }
            if (i < nseg - 1) {
                if (dup2(pipes[i][1], STDOUT_FILENO) < 0) { perror("dup2"); _exit(1); }
            }
            /* close all pipe fds in child */
            for (int j = 0; j < nseg - 1; ++j) { close(pipes[j][0]); close(pipes[j][1]); }

            /* Find redirection tokens for this segment */
            char *infile = find_redir_filename(tokens[i], ntokens[i], "<");
            char *outfile = find_redir_filename(tokens[i], ntokens[i], ">");

            if (infile) {
                int fd = open(infile, O_RDONLY);
                if (fd < 0) { fprintf(stderr, "Failed to open input '%s': %s\n", infile, strerror(errno)); _exit(127); }
                if (dup2(fd, STDIN_FILENO) < 0) { perror("dup2 infile"); close(fd); _exit(1); }
                close(fd);
            }
            if (outfile) {
                int fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) { fprintf(stderr, "Failed to open output '%s': %s\n", outfile, strerror(errno)); _exit(127); }
                if (dup2(fd, STDOUT_FILENO) < 0) { perror("dup2 outfile"); close(fd); _exit(1); }
                close(fd);
            }

            /* Build exec argv (compact) */
            char *exec_argv[MAX_TOKENS];
            int argc = build_exec_argv_from_tokens(tokens[i], ntokens[i], exec_argv);
            if (argc == 0) _exit(0);

            execvp(exec_argv[0], exec_argv);
            fprintf(stderr, "%s: command not found\n", exec_argv[0]);
            _exit(127);
        }

        /* Parent */
        pids[i] = pid;

        /* close ends not needed by parent */
        if (i > 0) close(pipes[i-1][0]);
        if (i < nseg - 1) close(pipes[i][1]);
    }

    /* parent: close any remaining pipe fds */
    for (int j = 0; j < nseg - 1; ++j) {
        close(pipes[j][0]); close(pipes[j][1]);
    }

    /* If background: track job leader (first pid) and return immediately.
       Before returning, free any allocated expansion strings in parent. */
    if (background) {
        add_job(pids[0], command);
        for (int a = 0; a < nseg; ++a) for (int b = 0; b < ntokens[a]; ++b) if (expanded_flags[a][b]) free(tokens[a][b]);
        free(line);
        return;
    }

    /* Foreground: wait for all children, then free expansions */
    int status;
    for (int i = 0; i < nseg; ++i) {
        waitpid(pids[i], &status, 0);
    }

    for (int a = 0; a < nseg; ++a) for (int b = 0; b < ntokens[a]; ++b) if (expanded_flags[a][b]) free(tokens[a][b]);

    free(line);
}

