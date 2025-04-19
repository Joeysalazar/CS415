// command.c
#include "command.h"
#include <unistd.h>     // getcwd(), write(), chdir(), unlink()
#include <stdlib.h>     // malloc(), free()
#include <limits.h>     // PATH_MAX
#include <string.h>     // strlen()
#include <dirent.h>     // opendir(), readdir(), closedir()
#include <fcntl.h>      // open()
#include <sys/stat.h>   // mkdir()
#include <errno.h>      // errno
#include <unistd.h>     // read(), write(), close()

// List Directory (ls)
void listDir() {
    DIR* d = opendir(".");
    if (!d) {
        const char *msg = "Error: failed to open directory\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        write(STDOUT_FILENO, entry->d_name, strlen(entry->d_name));
        write(STDOUT_FILENO, " ", 1);
    }
    closedir(d);
    write(STDOUT_FILENO, "\n", 1);
}

// Show Current Directory (pwd)
void showCurrentDir() {
    char *cwd = malloc(PATH_MAX);
    if (!cwd) {
        const char *msg = "Error: malloc failed in pwd\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        return;
    }
    if (getcwd(cwd, PATH_MAX) == NULL) {
        const char *msg = "Error: getcwd failed\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        free(cwd);
        return;
    }
    write(STDOUT_FILENO, cwd, strlen(cwd));
    write(STDOUT_FILENO, "\n", 1);
    free(cwd);
}

// Make Directory (mkdir)
void makeDir(char *dirName) {
    if (mkdir(dirName, 0755) < 0) {
        if (errno == EEXIST) {
            const char *msg = "Directory already exists!\n";
            write(STDOUT_FILENO, msg, strlen(msg));
        } else {
            const char *msg = "Error creating directory\n";
            write(STDOUT_FILENO, msg, strlen(msg));
        }
    }
}

// Change Directory (cd)
void changeDir(char *dirName) {
    if (chdir(dirName) < 0) {
        const char *msg = "Error! Failed to change directory\n";
        write(STDOUT_FILENO, msg, strlen(msg));
    }
}

// Copy File (cp)
void copyFile(char *sourcePath, char *destinationPath) {
    int src = open(sourcePath, O_RDONLY);
    if (src < 0) {
        const char *msg = "Error! Failed to open source file\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        return;
    }
    int dst = open(destinationPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst < 0) {
        const char *msg = "Error! Failed to open destination file\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        close(src);
        return;
    }
    char buf[4096];
    ssize_t bytes;
    while ((bytes = read(src, buf, sizeof(buf))) > 0) {
        ssize_t written = 0;
        while (written < bytes) {
            ssize_t w = write(dst, buf + written, bytes - written);
            if (w < 0) {
                const char *errmsg = "Error writing to destination file\n";
                write(STDOUT_FILENO, errmsg, strlen(errmsg));
                close(src);
                close(dst);
                return;
            }
            written += w;
        }
    }
    if (bytes < 0) {
        const char *msg = "Error reading source file\n";
        write(STDOUT_FILENO, msg, strlen(msg));
    }
    close(src);
    close(dst);
}

// Move File (mv)
void moveFile(char *sourcePath, char *destinationPath) {
    copyFile(sourcePath, destinationPath);
    if (unlink(sourcePath) < 0) {
        const char *msg = "Error deleting source file during move\n";
        write(STDOUT_FILENO, msg, strlen(msg));
    }
}

// Delete File (rm)
void deleteFile(char *filename) {
    if (unlink(filename) < 0) {
        const char *msg = "Error! File does not exist\n";
        write(STDOUT_FILENO, msg, strlen(msg));
    }
}

// Display File (cat)
void displayFile(char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        const char *msg = "Error! Failed to open file for display\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        return;
    }
    char buf[4096];
    ssize_t bytes;
    while ((bytes = read(fd, buf, sizeof(buf))) > 0) {
        ssize_t written = 0;
        while (written < bytes) {
            ssize_t w = write(STDOUT_FILENO, buf + written, bytes - written);
            if (w < 0) {
                close(fd);
                return;
            }
            written += w;
        }
    }
    close(fd);
}

