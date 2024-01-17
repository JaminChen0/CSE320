#ifndef DEET2_H
#define DEET2_H

#include <sys/types.h>

extern int show_prompt;
void handle_sigchld(int sig);
void handle_sigint(int sig);
void read_command(char **buffer, size_t *size, int show_prompt);
void execute_command(char *command_line);


#endif
