#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#define MAX_LINE   1024
#define MAX_ARGS   64
#define MAX_PROCS  64

static pid_t pids[MAX_PROCS];
static int  np = 0, current = 0;

// read utime & stime from /proc/[pid]/stat
static void report_usage(pid_t pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE *f = fopen(path, "r");
    if (!f) {
        perror("fopen /proc stat");
        return;
    }
    int pid_i; char comm[64], state;
    unsigned long utime, stime;
    // pid, comm, state, skip 11 fields, then utime & stime
    fscanf(f, "%d %63s %c %*s %*s %*s %*s %*s %*s %*s %*s %*s %lu %lu",
           &pid_i, comm, &state, &utime, &stime);
    fclose(f);
    printf("PID %d (%s): utime=%lu, stime=%lu\n",
           pid_i, comm, utime, stime);
}

// SIGALRM handler: report, stop current, advance, continue next, rearm
static void schedule_handler(int sig) {
    (void)sig;
    report_usage(pids[current]);
    if (np > 1) {
        kill(pids[current], SIGSTOP);
        current = (current + 1) % np;
        printf("MCP v4.0: scheduling PID %d\n", pids[current]);
        kill(pids[current], SIGCONT);
    }
    alarm(1);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input-file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // 1) Read commands and fork children (stopped immediately)
    FILE *fp = fopen(argv[1], "r");
    if (!fp) { perror("fopen"); exit(EXIT_FAILURE); }

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = '\0';
        if (!*line) continue;

        char *args[MAX_ARGS];
        int ac = 0;
        char *tok = strtok(line, " ");
        while (tok && ac < MAX_ARGS - 1) {
            args[ac++] = tok;
            tok = strtok(NULL, " ");
        }
        args[ac] = NULL;

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            break;
        }
        if (pid == 0) {
            kill(getpid(), SIGSTOP);
            execvp(args[0], args);
            perror(args[0]);
            exit(EXIT_FAILURE);
        }
        pids[np++] = pid;
    }
    fclose(fp);

    if (np == 0) {
        fprintf(stderr, "No commands to run.\n");
        exit(EXIT_FAILURE);
    }

    // 2) Install SIGALRM handler
    struct sigaction sa;
    sa.sa_handler = schedule_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGALRM, &sa, NULL);

    // 3) Start the first child and arm the clock
    printf("MCP v4.0: starting PID %d\n", pids[0]);
    kill(pids[0], SIGCONT);
    alarm(1);

    // 4) Wait for all children to exit
    int remaining = np;
    while (remaining > 0) {
        int status;
        pid_t w = wait(&status);
        if (w > 0) remaining--;
    }

    printf("MCP v4.0: all done, exiting\n");
    return 0;
}
