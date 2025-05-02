#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "command.h"

// Global file pointer used for dual output.
FILE *outFile = NULL;

// printBoth() prints the given message to STDOUT (the console)
// and also writes it to outFile ("output.txt").
void printBoth(const char *msg) {
    fputs(msg, stdout);
    if (outFile)
        fputs(msg, outFile);
}

int main(void) {
    // Open "output.txt" for writing.
    outFile = fopen("output.txt", "w");
    if (outFile == NULL) {
        perror("fopen output.txt failed");
        exit(EXIT_FAILURE);
    }
    
    // Display prompt and write it to both destinations.
    printBoth(">>> ");
    fflush(stdout);
    
    // Read one line of user input.
    char input[256];
    if (fgets(input, sizeof(input), stdin) == NULL) {
        printBoth("Error: reading input failed\n");
        fclose(outFile);
        exit(EXIT_FAILURE);
    }
    // Remove trailing newline.
    input[strcspn(input, "\n")] = '\0';
    
    // Process the command.
    if (strcmp(input, "lfcat") == 0) {
        lfcat();
    } else if (strcmp(input, "exit") == 0) {
        printBoth("Exiting...\n");
    } else {
        char errMsg[512];
        snprintf(errMsg, sizeof(errMsg), "Error: Unrecognized command '%s'\n", input);
        printBoth(errMsg);
    }
    
    fclose(outFile);
    return 0;
}

