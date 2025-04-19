// main.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "command.h"
#include "string_parser.h"

void run_interactive_mode();
void run_file_mode(const char *filename);
void process_line(char *line, FILE *output_stream, int *should_exit);

int main(int argc, char *argv[]) {
    if (argc == 1) {
        run_interactive_mode();
    } else if (argc == 3 && strcmp(argv[1], "-f") == 0) {
        run_file_mode(argv[2]);
    } else {
        fprintf(stderr, "Usage: %s [-f filename]\n", argv[0]);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

// Interactive mode: read from stdin, prompt “>>>”
void run_interactive_mode() {
    char *line = NULL;
    size_t len = 0;
    ssize_t nread;
    int should_exit = 0;

    while (!should_exit) {
        printf(">>> ");
        fflush(stdout);

        nread = getline(&line, &len, stdin);
        if (nread == -1) break;
        if (line[nread - 1] == '\n') line[nread - 1] = '\0';

        if (strcmp(line, "exit") == 0) break;
        process_line(line, stdout, &should_exit);
    }
    free(line);
}

// File mode: read from batch file, write **only** to output.txt
void run_file_mode(const char *filename) {
    FILE *in = fopen(filename, "r");
    if (!in) { perror("Failed to open input file"); exit(EXIT_FAILURE); }
    FILE *out = fopen("output.txt", "w");
    if (!out) { perror("Failed to open output file"); fclose(in); exit(EXIT_FAILURE); }

    char *line = NULL;
    size_t len = 0;
    ssize_t nread;
    int should_exit = 0;

    while (!should_exit && (nread = getline(&line, &len, in)) != -1) {
        if (line[nread - 1] == '\n') line[nread - 1] = '\0';
        if (strcmp(line, "exit") == 0) break;
        process_line(line, out, &should_exit);
    }
    free(line);
    fclose(in);
    fclose(out);
}

// Parse a line by “;”, tokenize via string_parser, dispatch commands
void process_line(char *line, FILE *output_stream, int *should_exit) {
    char *saveptr, *segment = strtok_r(line, ";", &saveptr);
    while (segment != NULL && !*should_exit) {
        // trim leading spaces
        while (*segment == ' ') segment++;

        // tokenize on spaces
        command_line cmd = str_filler(segment, " ");
        if (cmd.num_token == 0) {
            free_command_line(&cmd);
            segment = strtok_r(NULL, ";", &saveptr);
            continue;
        }

        // in‐line exit
        if (strcmp(cmd.command_list[0], "exit") == 0) {
            *should_exit = 1;
            free_command_line(&cmd);
            break;
        }

        // redirect stdout if writing to file
        int saved = -1;
        if (output_stream != stdout) {
            saved = dup(STDOUT_FILENO);
            dup2(fileno(output_stream), STDOUT_FILENO);
        }

        // dispatch
        char *c = cmd.command_list[0];
        if      (!strcmp(c,"pwd"))   { 
            if (cmd.num_token != 1) fprintf(output_stream,"Error! Incorrect number of arguments for pwd\n");
            else showCurrentDir();
        }
        else if (!strcmp(c,"ls"))    { 
            if (cmd.num_token != 1) fprintf(output_stream,"Error! Incorrect number of arguments for ls\n");
            else listDir();
        }
        else if (!strcmp(c,"mkdir")) { 
            if (cmd.num_token != 2) fprintf(output_stream,"Error! Incorrect number of arguments for mkdir\n");
            else makeDir(cmd.command_list[1]);
        }
        else if (!strcmp(c,"cd"))    { 
            if (cmd.num_token != 2) fprintf(output_stream,"Error! Incorrect number of arguments for cd\n");
            else changeDir(cmd.command_list[1]);
        }
        else if (!strcmp(c,"cp"))    { 
            if (cmd.num_token != 3) fprintf(output_stream,"Error! Incorrect number of arguments for cp\n");
            else copyFile(cmd.command_list[1], cmd.command_list[2]);
        }
        else if (!strcmp(c,"mv"))    { 
            if (cmd.num_token != 3) fprintf(output_stream,"Error! Incorrect number of arguments for mv\n");
            else moveFile(cmd.command_list[1], cmd.command_list[2]);
        }
        else if (!strcmp(c,"rm"))    { 
            if (cmd.num_token != 2) fprintf(output_stream,"Error! Incorrect number of arguments for rm\n");
            else deleteFile(cmd.command_list[1]);
        }
        else if (!strcmp(c,"cat"))   { 
            if (cmd.num_token != 2) fprintf(output_stream,"Error! Incorrect number of arguments for cat\n");
            else displayFile(cmd.command_list[1]);
        }
        else {
            fprintf(output_stream,"Error! Unrecognized command: %s\n", c);
        }

        // restore stdout
        if (saved != -1) {
            dup2(saved, STDOUT_FILENO);
            close(saved);
        }

        free_command_line(&cmd);
        segment = strtok_r(NULL, ";", &saveptr);
    }
}

