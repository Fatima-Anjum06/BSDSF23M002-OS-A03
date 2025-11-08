/* src/execute.c -- supports <, >, pipes, and background jobs */
#define _GNU_SOURCE
#include "../include/shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>

/* ---------- Job management ---------- */

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

/* reap any finished background children (non-blocking). Removes from job list and prints status */
void reap_background_jobs(void) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        /* If this pid is in our job list, print and remove it */
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

/* ---------- Parsing helpers (same style as Feature-5) ---------- */

/* trim whitespace in-place */
static void trim(char *s) {
    if (!s) return;
    while (*s && (*s == ' ' || *s == '\t')) memmove(s, s+1, strlen(s));
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' || s[len-1]=='\r' || s[len-1]=='\n')) {
        s[len-1] = '\0';
        len--;
    }
}

/* split pipeline into segments separated by '|' */
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

/* parse a segment: identify argv[], infile, outfile */
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

/* ---------- Built-ins (jobs handled here) ---------- */
int handle_builtin(char **args) {
    if (!args || !args[0]) return 0;
    if (strcmp(args[0], "help") == 0) {
        print_help();
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
    if (strcmp(args[0], "exit") == 0) {
        /* exit handled in shell loop */
        return 1;
    }
    return 0; /* not built-in */
}

void print_help(void) {
    printf("Built-in commands:\n");
    printf("  cd <dir>    Change directory\n");
    printf("  help        Show this message\n");
    printf("  jobs        List background jobs\n");
    printf("  exit        Exit the shell\n");
    printf("Notes: use & to run background, ; to chain commands, | and <, > supported.\n");
}

/* ---------- Main execute_command (supports background flag) ---------- */

void execute_command(char *command, int background) {
    if (!command || command[0] == '\0') return;

    /* duplicate because we will strtok_r */
    char *line = strdup(command);
    if (!line) { perror("strdup"); return; }

    /* split into pipeline segments */
    char *segments[MAX_PIPELINE];
    int nseg = split_pipeline(line, segments, MAX_PIPELINE);
    if (nseg <= 0) { free(line); return; }

    /* parse each segment */
    char *argvs[MAX_PIPELINE][MAX_TOKENS];
    char *infiles[MAX_PIPELINE];
    char *outfiles[MAX_PIPELINE];
    int argcs[MAX_PIPELINE];

    for (int i = 0; i < nseg; ++i) {
        argcs[i] = parse_segment(segments[i], argvs[i], &infiles[i], &outfiles[i]);
        if (argcs[i] < 0) { free(line); return; }
        if (argcs[i] == 0) { fprintf(stderr, "Syntax error: empty command in pipeline\n"); free(line); return; }
    }

    /* If single segment and builtin -> handle in parent (no fork) */
    if (nseg == 1) {
        if (handle_builtin(argvs[0])) { free(line); return; }
    }

    /* create pipes */
    int pipes[MAX_PIPELINE-1][2];
    for (int i = 0; i < nseg-1; ++i) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
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
            for (int j = 0; j < nseg-1; ++j) { close(pipes[j][0]); close(pipes[j][1]); }
            free(line);
            return;
        }

        if (pid == 0) {
            /* Child */

            /* if not first, dup previous pipe read to stdin */
            if (i > 0) {
                if (dup2(pipes[i-1][0], STDIN_FILENO) < 0) { perror("dup2"); exit(1); }
            }
            /* if not last, dup pipe write to stdout */
            if (i < nseg-1) {
                if (dup2(pipes[i][1], STDOUT_FILENO) < 0) { perror("dup2"); exit(1); }
            }

            /* close all pipe fds in child */
            for (int j = 0; j < nseg-1; ++j) { close(pipes[j][0]); close(pipes[j][1]); }

            /* handle infile */
            if (infiles[i]) {
                int fd = open(infiles[i], O_RDONLY);
                if (fd < 0) { fprintf(stderr, "Failed to open %s: %s\n", infiles[i], strerror(errno)); exit(127); }
                if (dup2(fd, STDIN_FILENO) < 0) { perror("dup2 infile"); close(fd); exit(1); }
                close(fd);
            }

            /* handle outfile (truncate) */
            if (outfiles[i]) {
                int fd = open(outfiles[i], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) { fprintf(stderr, "Failed to open %s: %s\n", outfiles[i], strerror(errno)); exit(127); }
                if (dup2(fd, STDOUT_FILENO) < 0) { perror("dup2 outfile"); close(fd); exit(1); }
                close(fd);
            }

            /* execute */
            execvp(argvs[i][0], argvs[i]);
            fprintf(stderr, "%s: command not found\n", argvs[i][0]);
            exit(127);
        }

        /* parent records pid */
        pids[i] = pid;

        /* parent closes unnecessary pipe ends */
        if (i > 0) close(pipes[i-1][0]);
        if (i < nseg-1) close(pipes[i][1]);
    }

    /* parent should close any remaining pipe fds */
    for (int j = 0; j < nseg-1; ++j) {
        /* ignore errors; some ends may already be closed */
        close(pipes[j][0]);
        close(pipes[j][1]);
    }

    /* If background: do not wait; add job and return immediately */
    if (background) {
        /* we track the first child's pid as the job leader */
        add_job(pids[0], command);
        free(line);
        return;
    }

    /* Foreground: wait for all children */
    int status;
    for (int i = 0; i < nseg; ++i) {
        waitpid(pids[i], &status, 0);
    }

    free(line);
}
