#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_LINE   1024
#define MAX_ARGS   64
#define MAX_PROCS  64

static pid_t pids[MAX_PROCS];
static int  np = 0, current = 0;
static unsigned long prev_utime[MAX_PROCS], prev_stime[MAX_PROCS];
static unsigned int  slice_sec[MAX_PROCS];

// read utime & stime from /proc/[pid]/stat
static int read_proc_stat(int idx) {
    char path[64], buf[512];
    snprintf(path, sizeof(path), "/proc/%d/stat", pids[idx]);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    ssize_t n = read(fd, buf, sizeof(buf)-1);
    close(fd);
    if (n <= 0) return -1;
    buf[n] = '\0';
    unsigned long utime, stime;
    // parse: pid (comm) state ... utime stime ...
    // skip first 13 fields
    char *s = buf;
    for (int i = 0; i < 13; i++) {
        s = strchr(s, ' ');
        if (!s) return -1;
        s++;
    }
    if (sscanf(s, "%lu %lu", &utime, &stime) != 2) return -1;
    prev_utime[idx] = utime;
    prev_stime[idx] = stime;
    return 0;
}

// SIGALRM handler: adaptive slice and context switch
static void schedule_handler(int sig) {
    (void)sig;
    int idx = current;
    unsigned long old_ut = prev_utime[idx], old_st = prev_stime[idx];
    // get new stats
    read_proc_stat(idx);
    unsigned long new_ut = prev_utime[idx], new_st = prev_stime[idx];
    unsigned long delta_ut = new_ut - old_ut;
    unsigned long delta_st = new_st - old_st;

    // classify and choose next slice
    if (delta_ut > delta_st) {
        slice_sec[idx] = 2;  // CPU-bound: longer slice
        printf("PID %d CPU-bound; next slice %us\n", pids[idx], slice_sec[idx]);
    } else {
        slice_sec[idx] = 1;  // I/O-bound: shorter slice
        printf("PID %d I/O-bound; next slice %us\n", pids[idx], slice_sec[idx]);
    }

    // stop current
    kill(pids[idx], SIGSTOP);

    // advance
    current = (current + 1) % np;
    printf("MCP v5.0: switching to PID %d\n", pids[current]);

    // start next
    kill(pids[current], SIGCONT);
    // read its initial stats before running
    read_proc_stat(current);
    // arm timer with its slice
    alarm(slice_sec[current]);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input-file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // 1) Fork children (stopped immediately) and init slices
    FILE *fp = fopen(argv[1], "r");
    if (!fp) { perror("fopen"); exit(EXIT_FAILURE); }

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = '\0';
        if (!*line) continue;

        char *args[MAX_ARGS];
        int ac = 0;
        char *tok = strtok(line, " ");
        while (tok && ac < MAX_ARGS -1) {
            args[ac++] = tok;
            tok = strtok(NULL, " ");
        }
        args[ac] = NULL;

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork"); break;
        }
        if (pid == 0) {
            kill(getpid(), SIGSTOP);
            execvp(args[0], args);
            perror(args[0]);
            exit(EXIT_FAILURE);
        }
        pids[np] = pid;
        slice_sec[np] = 1;               // default 1s
        prev_utime[np] = prev_stime[np] = 0;
        np++;
    }
    fclose(fp);

    if (np == 0) {
        fprintf(stderr, "No commands to run.\n");
        exit(EXIT_FAILURE);
    }

    // 2) Install SIGALRM handler
    struct sigaction sa = { .sa_handler = schedule_handler };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGALRM, &sa, NULL);

    // 3) Initialize first child
    printf("MCP v5.0: starting PID %d\n", pids[0]);
    // read its stats
    read_proc_stat(0);
    kill(pids[0], SIGCONT);
    alarm(slice_sec[0]);

    // 4) Wait until all children exit
    int remaining = np;
    while (remaining > 0) {
        int status;
        pid_t w = wait(&status);
        if (w > 0) remaining--;
    }

    printf("MCP v5.0: all done, exiting\n");
    return 0;
}
