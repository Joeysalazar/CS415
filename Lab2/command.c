#include "command.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>     // for write(), STDOUT_FILENO, getcwd()
#include <string.h>     // for strlen(), snprintf(), strcmp()
#include <dirent.h>     // for opendir(), readdir(), closedir()
#include <limits.h>     // for PATH_MAX
#include <sys/stat.h>   // for stat(), S_ISREG()

// Declare outFile as external – defined in main.c.
extern FILE *outFile;

// printBoth() here is the same helper as in main.c.
static void printBoth(const char *msg) {
    fputs(msg, stdout);
    if (outFile)
        fputs(msg, outFile);
}

// Define a separator line (80 dashes + newline).
#define SEP_LINE "--------------------------------------------------------------------------------\n"

// Helper: determine whether a file should be excluded from processing.
static int should_exclude(const char *filename) {
    if (!filename)
        return 1;
    if (strcmp(filename, ".") == 0 ||
        strcmp(filename, "..") == 0 ||
        strcmp(filename, "main.c") == 0 ||
        strcmp(filename, "command.c") == 0 ||
        strcmp(filename, "command.h") == 0 ||
        strcmp(filename, "Makefile") == 0 ||
        strcmp(filename, "output.txt") == 0 ||
        strcmp(filename, "lab2") == 0 ||
        strcmp(filename, "lab2.exe") == 0)
        return 1;
    return 0;
}

// Helper: process all eligible files in directory dir_path.
static void process_directory(const char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Error: opendir(%s) failed\n", dir_path);
        printBoth(msg);
        return;
    }
    
    // To enforce a sorted order, we collect file names in an array.
    int capacity = 10;
    int count = 0;
    char **files = malloc(capacity * sizeof(char *));
    if (!files) {
        closedir(dir);
        return;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (should_exclude(entry->d_name))
            continue;
            
        char filepath[PATH_MAX];
        snprintf(filepath, sizeof(filepath), "%s/%s", dir_path, entry->d_name);
        struct stat st;
        if (stat(filepath, &st) == -1)
            continue;
        if (!S_ISREG(st.st_mode))
            continue;
            
        // Add file name to array.
        if (count >= capacity) {
            capacity *= 2;
            char **temp = realloc(files, capacity * sizeof(char *));
            if (!temp)
                break;
            files = temp;
        }
        files[count] = strdup(entry->d_name);
        if (!files[count])
            continue;
        count++;
    }
    closedir(dir);
    
    // Sort files alphabetically.
    int cmpfunc(const void *a, const void *b) {
         const char *s1 = *(const char **)a;
         const char *s2 = *(const char **)b;
         return strcmp(s1, s2);
    }
    qsort(files, count, sizeof(char *), cmpfunc);
    
    // Process each file.
    for (int i = 0; i < count; i++) {
        char filepath[PATH_MAX];
        snprintf(filepath, sizeof(filepath), "%s/%s", dir_path, files[i]);
        
        char header[512];
        snprintf(header, sizeof(header), "File: %s\n", files[i]);
        printBoth(header);
        
        FILE *f = fopen(filepath, "r");
        if (!f) {
            printBoth("Error: cannot open file\n");
            free(files[i]);
            continue;
        }
        
        char *line = NULL;
        size_t len = 0;
        ssize_t nread;
        while ((nread = getline(&line, &len, f)) != -1) {
            printBoth(line);
        }
        free(line);
        fclose(f);
        
        printBoth(SEP_LINE);
        free(files[i]);
    }
    free(files);
}

// lfcat(): Check for a subdirectory called "files". If it exists, process that directory; otherwise, process the current directory.
void lfcat() {
    DIR *testDir = opendir("files");
    if (testDir) {
        closedir(testDir);
        process_directory("files");
    } else {
        char cwd[PATH_MAX];
        if (!getcwd(cwd, sizeof(cwd))) {
            printBoth("Error: getcwd() failed\n");
            return;
        }
        process_directory(cwd);
    }
}

