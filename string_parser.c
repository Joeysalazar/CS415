/*
 * string_parser.c
 *
 *  Created on: Nov 25, 2020
 *      Author: gguan, Monil
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "string_parser.h"

#define _GNU_SOURCE

int count_token(char* buf, const char* delim)
{
        //TODO：
        /*
        *       #1.     Check for NULL string
        *       #2.     iterate through string counting tokens
        *               Cases to watchout for
        *                       a.      string start with delimeter
        *                       b.      string end with delimeter
        *                       c.      account NULL for the last token
        *       #3. return the number of token (note not number of delimeter)
        */
        if (buf == NULL || delim == NULL)
                return 0;
                
        // Duplicate the string so the original isn't modified.
        char* buf_copy = strdup(buf);
        if (buf_copy == NULL)
                return 0;
                
        int count = 0;
        char *token;
        char *saveptr;
        
        token = strtok_r(buf_copy, delim, &saveptr);
        while (token != NULL)
        {
                count++;
                token = strtok_r(NULL, delim, &saveptr);
        }
        free(buf_copy);
        return count;
}

command_line str_filler(char* buf, const char* delim)
{
        //TODO：
        /*
        *       #1.     create command_line variable to be filled and returned
        *       #2.     count the number of tokens with count_token function, set num_token.
        *               one can use strtok_r to remove the \n at the end of the line.
        *       #3. malloc memory for token array inside command_line variable
        *               based on the number of tokens.
        *       #4.     use function strtok_r to find out the tokens
        *       #5. malloc each index of the array with the length of tokens,
        *               fill command_list array with tokens, and fill last spot with NULL.
        *       #6. return the variable.
        */
        command_line cmd;
        
        // Remove trailing newline, if any.
        if (buf != NULL)
        {
                size_t len = strlen(buf);
                if (len > 0 && buf[len - 1] == '\n')
                        buf[len - 1] = '\0';
        }
        
        // Count tokens in the string.
        cmd.num_token = count_token(buf, delim);
        
        // Allocate memory for the token array (plus one extra for the NULL termination).
        cmd.command_list = (char**)malloc((cmd.num_token + 1) * sizeof(char*));
        if (cmd.command_list == NULL)
        {
                cmd.num_token = 0;
                return cmd;
        }
        
        int index = 0;
        char *token;
        char *saveptr;
        
        // Tokenize the input string.
        token = strtok_r(buf, delim, &saveptr);
        while (token != NULL)
        {
                // Allocate memory for the token and copy it.
                cmd.command_list[index] = strdup(token);
                if (cmd.command_list[index] == NULL)
                {
                        // If allocation fails, free any already allocated tokens.
                        for (int i = 0; i < index; i++)
                        {
                                free(cmd.command_list[i]);
                        }
                        free(cmd.command_list);
                        cmd.command_list = NULL;
                        cmd.num_token = 0;
                        return cmd;
                }
                index++;
                token = strtok_r(NULL, delim, &saveptr);
        }
        // Set the last element to NULL to mark the end of the array.
        cmd.command_list[index] = NULL;
        return cmd;
}

void free_command_line(command_line* command)
{
        //TODO：
        /*
        *       #1.     free the array base num_token
        */
        if (command == NULL || command->command_list == NULL)
                return;
                
        for (int i = 0; i < command->num_token; i++)
        {
                free(command->command_list[i]);
        }
        free(command->command_list);
        command->command_list = NULL;
}

