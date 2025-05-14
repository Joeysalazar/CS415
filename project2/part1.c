#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_LINE 1024
#define MAX_ARGS 64

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input-file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    FILE *fp = fopen(argv[1], "r");
    if (!fp) { perror("fopen"); exit(EXIT_FAILURE); }

    char line[MAX_LINE];
    pid_t pids[MAX_ARGS];
    int np = 0;

    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = '\0';
        if (!*line) continue;

        char *args[MAX_ARGS];
        int ac = 0;
        char *tok = strtok(line, " ");
        while (tok && ac < MAX_ARGS-1) {
            args[ac++] = tok;
            tok = strtok(NULL, " ");
        }
        args[ac] = NULL;

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            continue;
        }
        if (pid == 0) {
            execvp(args[0], args);
            perror(args[0]);
            exit(EXIT_FAILURE);
        }
        pids[np++] = pid;
    }
    fclose(fp);

    for (int i = 0; i < np; i++) {
        waitpid(pids[i], NULL, 0);
    }

    printf("MCP v1.0: all done, exiting\n");
    return 0;
}
