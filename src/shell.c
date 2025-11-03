#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "shell.h"

/* ---------- Startup, prompt and input ---------- */

void init_shell() {
    printf("\n*** Welcome to MyShell ***\n");
    printf("*** Type 'help' to see available commands ***\n\n");

    init_history();
}

void print_prompt() {
    printf("myshell> ");
    fflush(stdout);
}

void read_command(char *command) {
    if (fgets(command, MAX_COMMAND_LEN, stdin) == NULL) {
        /* EOF (Ctrl-D) — exit gracefully */
        printf("\n");
        command[0] = '\0';
        return;
    }

    /* Remove trailing newline if present */
    size_t len = strlen(command);
    if (len > 0 && command[len - 1] == '\n') {
        command[len - 1] = '\0';
    }
}

/* ---------- History implementation (circular buffer) ---------- */

static char *history_buf[HISTORY_SIZE];
static int hist_count = 0;         /* how many entries currently stored (<= HISTORY_SIZE) */
static int hist_next = 0;          /* index where next entry will be written (0..HISTORY_SIZE-1) */
static long hist_total = 0;        /* total commands ever added (monotonic), used for sequence numbers */

/* Initialize history buffer */
void init_history(void) {
    for (int i = 0; i < HISTORY_SIZE; ++i) {
        history_buf[i] = NULL;
    }
    hist_count = 0;
    hist_next = 0;
    hist_total = 0;
}

/* Add a command string to history (makes a strdup copy) */
void add_history_entry(const char *cmd) {
    if (cmd == NULL || cmd[0] == '\0') return;

    /* Free slot if it's occupied (we are going to overwrite when full) */
    if (history_buf[hist_next] != NULL) {
        free(history_buf[hist_next]);
        history_buf[hist_next] = NULL;
    }

    history_buf[hist_next] = strdup(cmd);
    if (history_buf[hist_next] == NULL) {
        perror("strdup");
        return;
    }

    hist_next = (hist_next + 1) % HISTORY_SIZE;
    if (hist_count < HISTORY_SIZE) hist_count++;
    hist_total++;
}

/* Print history lines with sequence numbers starting at 1 for first stored command.
   Sequence numbering uses hist_total to create monotonically increasing numbers. */
void print_history(void) {
    if (hist_count == 0) {
        printf("No history.\n");
        return;
    }

    /* Compute the sequence number of the oldest entry stored */
    long start_seq = hist_total - hist_count + 1; /* e.g., if hist_total=5 and hist_count=5 -> start_seq=1 */

    /* Compute index of oldest entry in circular buffer */
    int oldest_index = (hist_next - hist_count + HISTORY_SIZE) % HISTORY_SIZE;

    for (int i = 0; i < hist_count; ++i) {
        int idx = (oldest_index + i) % HISTORY_SIZE;
        long seq = start_seq + i;
        printf("%3ld  %s\n", seq, history_buf[idx]);
    }
}

/* Return pointer to command by its sequence number (as printed by print_history).
   The returned pointer points into our internal buffer — caller must not free it.
   Return NULL if seq_num is out of range. */
const char *get_history_command(int seq_num) {
    if (hist_count == 0) return NULL;

    long start_seq = hist_total - hist_count + 1;
    long end_seq = hist_total;

    if (seq_num < start_seq || seq_num > end_seq) return NULL;

    long offset = seq_num - start_seq; /* 0-based offset into stored entries */
    int oldest_index = (hist_next - hist_count + HISTORY_SIZE) % HISTORY_SIZE;
    int idx = (oldest_index + (int)offset) % HISTORY_SIZE;
    return history_buf[idx];
}

/* Optional: cleanup on exit (not required, but nice) */
void cleanup_history(void) {
    for (int i = 0; i < HISTORY_SIZE; ++i) {
        if (history_buf[i]) {
            free(history_buf[i]);
            history_buf[i] = NULL;
        }
    }
    hist_count = 0;
    hist_next = 0;
    hist_total = 0;
}
