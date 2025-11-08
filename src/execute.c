/* src/execute.c -- supports <, >, and pipes | */
#define _GNU_SOURCE
#include "../include/shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>

/* Helpers */

/* trim whitespace in-place */
static void trim(char *s) {
    if (!s) return;
    while (*s && (*s == ' ' || *s == '\t')) memmove(s, s+1, strlen(s));
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' || s[len-1] == '\r' || s[len-1]=='\n')) {
        s[len-1] = '\0';
        len--;
    }
}

/* split pipeline into segments (modifies input) */
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

/* parse one segment: fill argv[], and set infile/outfile if present */
static int parse_segment(char *seg, char *argv[], char **infile, char **outfile) {
    *infile = NULL;
    *outfile = NULL;
    int argc = 0;
    char *saveptr = NULL;
    char *tok = strtok_r(seg, " \t", &saveptr);
    while (tok && argc < MAX_TOKENS-1) {
        if (strcmp(tok, "<") == 0) {
            tok = strtok_r(NULL, " \t", &saveptr);
            if (!tok) { fprintf(stderr, "Syntax error: expected filename after '<'\n"); return -1; }
            *infile = tok;
        } else if (strcmp(tok, ">") == 0) {
            tok = strtok_r(NULL, " \t", &saveptr);
            if (!tok) { fprintf(stderr, "Syntax error: expected filename after '>'\n"); return -1; }
            *outfile = tok;
        } else {
            argv[argc++] = tok;
        }
        tok = strtok_r(NULL, " \t", &saveptr);
    }
    argv[argc] = NULL;
    return argc;
}

/* convert argv array into a printable command for error messages */
static const char *null_or_str(char *s) { return s ? s : "(null)"; }

/* Built-in handler: tokenizes a single segment and executes builtins,
   returns 1 if built-in handled, 0 otherwise.
   Note: built-ins are only supported for single-segment commands (no pipe). */
int handle_builtin(char **args) {
    if (!args || !args[0]) return 0;

    if (strcmp(args[0], "help") == 0) {
        print_help();
        return 1;
    }
    if (strcmp(args[0], "cd") == 0) {
        if (!args[1]) {
            fprintf(stderr, "cd: expected argument\n");
        } else if (chdir(args[1]) != 0) {
            perror("cd");
        }
        return 1;
    }
    if (strcmp(args[0], "jobs") == 0) {
        printf("Job control not yet implemented.\n");
        return 1;
    }
    /* history and !n are handled by readline + shell loop; skip here */
    return 0;
}

void print_help(void) {
    printf("Built-in commands:\n");
    printf("  cd <dir>    Change directory\n");
    printf("  help        Show this message\n");
    printf("  jobs        Placeholder for job control\n");
    printf("  exit        Exit the shell\n");
    printf("Note: redirection: <, > and pipes: | are supported.\n");
}

/* Main execution function */
void execute_command(char *command) {
    if (!command || command[0] == '\0') return;

    /* make a mutable copy */
    char *line = strdup(command);
    if (!line) { perror("strdup"); return; }

    /* split pipeline */
    char *segments[MAX_PIPELINE];
    int nseg = split_pipeline(line, segments, MAX_PIPELINE);
    if (nseg <= 0) { free(line); return; }

    /* per-segment parse */
    char *argvs[MAX_PIPELINE][MAX_TOKENS];
    char *infiles[MAX_PIPELINE];
    char *outfiles[MAX_PIPELINE];
    int argcs[MAX_PIPELINE];

    for (int i = 0; i < nseg; ++i) {
        argcs[i] = parse_segment(segments[i], argvs[i], &infiles[i], &outfiles[i]);
        if (argcs[i] < 0) { free(line); return; }
        if (argcs[i] == 0) { fprintf(stderr, "Syntax error: empty command in pipeline\n"); free(line); return; }
    }

    /* If single segment and builtin -> handle directly (no fork) */
    if (nseg == 1) {
        if (handle_builtin(argvs[0])) { free(line); return; }
    }

    /* Create pipes */
    int pipes[MAX_PIPELINE - 1][2];
    for (int i = 0; i < nseg - 1; ++i) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            /* close previously opened pipes */
            for (int j = 0; j < i; ++j) { close(pipes[j][0]); close(pipes[j][1]); }
            free(line);
            return;
        }
    }

    pid_t pids[MAX_PIPELINE];
    for (int i = 0; i < nseg; ++i) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            /* close pipes */
            for (int j = 0; j < nseg - 1; ++j) { close(pipes[j][0]); close(pipes[j][1]); }
            free(line);
            return;
        }

        if (pid == 0) {
            /* CHILD process */

            /* If not first segment, set stdin to previous pipe read end */
            if (i > 0) {
                if (dup2(pipes[i-1][0], STDIN_FILENO) < 0) { perror("dup2"); exit(1); }
            }

            /* If not last, set stdout to this pipe write end */
            if (i < nseg - 1) {
                if (dup2(pipes[i][1], STDOUT_FILENO) < 0) { perror("dup2"); exit(1); }
            }

            /* Close all pipe fds in child */
            for (int j = 0; j < nseg - 1; ++j) { close(pipes[j][0]); close(pipes[j][1]); }

            /* Handle input redirection for this segment */
            if (infiles[i]) {
                int fd = open(infiles[i], O_RDONLY);
                if (fd < 0) {
                    fprintf(stderr, "Failed to open input '%s': %s\n", infiles[i], strerror(errno));
                    exit(127);
                }
                if (dup2(fd, STDIN_FILENO) < 0) { perror("dup2 infile"); close(fd); exit(1); }
                close(fd);
            }

            /* Handle output redirection for this segment (truncate) */
            if (outfiles[i]) {
                int fd = open(outfiles[i], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) {
                    fprintf(stderr, "Failed to open output '%s': %s\n", outfiles[i], strerror(errno));
                    exit(127);
                }
                if (dup2(fd, STDOUT_FILENO) < 0) { perror("dup2 outfile"); close(fd); exit(1); }
                close(fd);
            }

            /* Execute command */
            execvp(argvs[i][0], argvs[i]);
            /* If exec fails */
            fprintf(stderr, "%s: command not found\n", argvs[i][0]);
            exit(127);
        }

        /* parent */
        pids[i] = pid;

        /* Parent closes the write end of previous pipe and read end of this pipe as appropriate */
        if (i > 0) {
            close(pipes[i-1][0]); /* previous read end not needed in parent */
        }
        if (i < nseg - 1) {
            close(pipes[i][1]);   /* this write end not needed in parent */
        }
    }

    /* Close any remaining pipe fds in parent */
    for (int j = 0; j < nseg - 1; ++j) {
        /* some may already be closed; ignore errors */
        close(pipes[j][0]); close(pipes[j][1]);
    }

    /* Wait for all children */
    int status;
    for (int i = 0; i < nseg; ++i) {
        waitpid(pids[i], &status, 0);
    }

    free(line);
}
