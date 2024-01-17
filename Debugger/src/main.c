#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <signal.h>
#include <string.h>
#include "deet.h"
#include "deet2.h"

int main(int argc, char *argv[]) {
    // TO BE IMPLEMENTED
    // Remember: Do not put any functions other than main() in this file.
    log_startup();
    int show_prompt = 1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0) {
            show_prompt = 0;
            break;
        }
    }
    struct sigaction act, act_int;
    //SIGCHLD
    act.sa_handler=handle_sigchld;
    sigemptyset(&act.sa_mask);
    //act.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    act.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &act, NULL) == -1) {
        perror("sigaction SIGCHLD");
        exit(EXIT_FAILURE);
    }
    //SIGINT
    act_int.sa_handler = handle_sigint;
    sigemptyset(&act_int.sa_mask);
    act_int.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &act_int, NULL) == -1) {
        perror("sigaction SIGINT");
        exit(EXIT_FAILURE);
    }

    char *command=NULL;
    size_t len = 0;
    while (1) {
        //log_prompt();
        read_command(&command, &len,  show_prompt);
        if (strlen(command) > 255) {
            printf("Command is too long, reEnter gaga.\n");
            continue;
        }
        log_input(command);
        execute_command(command);
    }
    free(command);
    log_shutdown();
    return 0;
}
