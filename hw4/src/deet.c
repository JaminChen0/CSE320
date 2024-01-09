#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include "deet.h"
#include "deet2.h"
#include <errno.h>
#include <sys/user.h>

#define MAX_PROCESSES 100
//just left it here, don't want to move to head file.
typedef struct {
    pid_t pid;
    int deet_id;
    PSTATE state;
    int is_traced;
    char cmd_str[256];
    int exit_status;
} ProcessInfo;
ProcessInfo p[MAX_PROCESSES];
int num_processes = 0;
//pirnt
void p_info(const ProcessInfo *proce) {
    if (proce == NULL) {
        return;
    }
    const char *stateSt;
    char exit_status_str[32] = "";
    switch (proce->state) {
        case PSTATE_NONE:
            stateSt = "none";
            break;
        case PSTATE_RUNNING:
            stateSt = "running";
            break;
        case PSTATE_STOPPING:
            stateSt = "stopping";
            break;
        case PSTATE_STOPPED:
            stateSt = "stopped";
            break;
        case PSTATE_CONTINUING:
            stateSt = "continuing";
            break;
        case PSTATE_KILLED:
            stateSt = "killed";
            break;
        case PSTATE_DEAD:
            stateSt = "dead";
            if (WIFEXITED(proce->exit_status)) {
                snprintf(exit_status_str, sizeof(exit_status_str), "0x%x", WEXITSTATUS(proce->exit_status));
            } else if(WIFSIGNALED(proce->exit_status)) {
                snprintf(exit_status_str, sizeof(exit_status_str), "0x%x", WTERMSIG(proce->exit_status));
            }
            break;
        default:
            stateSt = "unknown";
    }
    printf("%d\t%d\t%c\t%s\t%s\t\t%s\n",
           proce->deet_id, proce->pid,
           proce->is_traced ? 'T' : 'U',
           stateSt,
           exit_status_str,
           proce->cmd_str);
}
//first read then execute
void read_command(char **buffer, size_t *size, int show_prompt) {
    while (1) {
        log_prompt();
        fflush(stdout);

        if (show_prompt) {
        //fflush(stdout);
        printf("deet> ");
        fflush(stdout);
        }
        /////////////////////////////////
        //printf("gasafsdffa");
        if (getline(buffer, size, stdin)== -1) {
            if (errno == EINTR) {
                clearerr(stdin);
                continue;
            } else if (feof(stdin)) {
                clearerr(stdin);
                strcpy(*buffer, "quit");
                break;
            } else {
                perror("getline");
                exit(EXIT_FAILURE);
            }
        }
        break;
    }
}

//here has problemmmmmmmmmmmmmmm
void update_process_status(pid_t pid, PSTATE new_state, int exit_status) {
    for (int i = 0; i < num_processes; i++) {
        if (p[i].pid == pid) {
            PSTATE old_state= p[i].state;
            p[i].state= new_state;
            p[i].exit_status = exit_status;

            log_state_change(pid, old_state, new_state, exit_status);
            p_info(&p[i]);
            break;
        }
    }
}
void handle_sigchld(int sig) {
    log_signal(sig);
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
            //stop
            if (WIFSTOPPED(status)) {
                update_process_status(pid, PSTATE_STOPPED, WSTOPSIG(status));
            //run
            } else if (WIFCONTINUED(status)) {
                update_process_status(pid, PSTATE_RUNNING, 0);
            //kill or ternimal
            } else {
                int exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : WTERMSIG(status);
                update_process_status(pid, PSTATE_DEAD, exit_status);
            }
    }

}

void quit() {
    //int status;
    //pid_t pid;
    // Send SIGKILL to all managed processes that are not already dead
    for (int i = 0; i < num_processes; i++) {
        if ((p[i].state != PSTATE_DEAD) && (p[i].state != PSTATE_KILLED) ) {
            PSTATE old_state = p[i].state;
            p[i].state = PSTATE_KILLED;
            log_state_change(p[i].pid, old_state, PSTATE_KILLED, 0);
            p_info(&p[i]);
            kill(p[i].pid, SIGKILL);
        }
    }
    // wait all dead
    int all_d;
    while (!all_d){
        all_d = 1;
        for (int i = 0; i < num_processes; i++) {
            if (p[i].state != PSTATE_DEAD) {
                all_d = 0;
                break;
            }
        }
        //10ms
        usleep(10000);
    }


}
//control + c
void handle_sigint(int sig) {
    log_signal(sig);
    for (int i = 0; i < num_processes; i++) {
        if (p[i].state != PSTATE_DEAD) {
            PSTATE old_state = p[i].state;
            p[i].state = PSTATE_KILLED;
            log_state_change(p[i].pid, old_state, PSTATE_KILLED, 0);
            p_info(&p[i]);

            kill(p[i].pid, SIGKILL);
        }
    }

    for (int i = 0; i < num_processes; i++) {
        if (p[i].state != PSTATE_DEAD) {
            int status;
            waitpid(p[i].pid, &status, 0);
        }
    }
    sleep(3);
    log_shutdown();
    exit(EXIT_SUCCESS);
}



void collect_dead() {
    for (int i = 0; i < num_processes; i++) {
        //deleted dead, but the didn't let deet id re useble
        if (p[i].state == PSTATE_DEAD) {
            p[i].pid = 0;
            p[i].deet_id = -1;
            p[i].state = PSTATE_NONE;
            p[i].is_traced = 0;
            p[i].exit_status = 0;
            memset(p[i].cmd_str, 0, sizeof(p[i].cmd_str));
        }
    }
}

// program: echo   argv: echo a b c
void run(char *program, char*const argv[]) {
    //comcat cmd_str
    char cmd_str[256]={0};
    //printf("Executing program: %s\n", program);
    for (int i = 0; argv[i] != NULL; i++) {
        //printf("Argument %d: %s\n", i, argv[i]);
        strcat(cmd_str, argv[i]);
        if (argv[i + 1] != NULL) {
            strcat(cmd_str, " ");
        }
    }
    collect_dead();

    //start
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        log_error("Forking son fail");
        return;
    }
    //son
    if (pid == 0) {
        if (dup2(STDERR_FILENO, STDOUT_FILENO) == -1) {
            perror("dup2");
            exit(EXIT_FAILURE);
        }
        // ptrace on
        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) == -1) {
            perror("ptrace");
            log_error("Ptrace fail");
            exit(EXIT_FAILURE);
        }
        //raise(SIGSTOP);
        sleep(2);

        //run too fast
        execvp(program, argv);

        perror("execvp");
        log_error("Executing son fail");
        exit(EXIT_FAILURE);
    } else {
        //father
        int n = num_processes;
        num_processes++;

        p[n].is_traced = 1;
        p[n].pid = pid;
        p[n].deet_id = n;
        p[n].state = PSTATE_RUNNING;
        strncpy(p[n].cmd_str, cmd_str, sizeof(p[n].cmd_str) - 1);

        log_state_change(pid, PSTATE_NONE, PSTATE_RUNNING, 0);
        p_info(&p[n]);

        ptrace(PTRACE_CONT, pid, NULL, NULL);
        //ptrace(PTRACE_CONT, pid, NULL, NULL);

        int status;
        waitpid(pid, &status, WUNTRACED);
        //printf("gagasjafosaodfjhsaodhfa");

        //wait if stop
        if (WIFSTOPPED(status)) {
            p[n].state = PSTATE_STOPPED;
            //p[n].is_traced = 1;
            log_state_change(pid, PSTATE_RUNNING, PSTATE_STOPPED, WSTOPSIG(status));
            p_info(&p[n]);
        }

    }
}

void stop(int deet_id) {
    for (int i = 0; i < num_processes; i++) {
        if (p[i].deet_id == deet_id && p[i].state == PSTATE_RUNNING) {
            // ptrace
            if (ptrace(PTRACE_INTERRUPT, p[i].pid, NULL, NULL) == -1) {
                perror("ptrace stop");
                log_error("Error stopping with ptrace");
                return;
            }
            p[i].state = PSTATE_STOPPING;
            log_state_change(p[i].pid, PSTATE_RUNNING, PSTATE_STOPPING, 0);
            p_info(&p[i]);
            break;
        }
    }
}

void continue1(int deet_id) {
    //sleep(1);
    for (int i = 0; i < num_processes; i++) {
        if (p[i].deet_id == deet_id && p[i].state == PSTATE_STOPPED) {

            //p[i].state = PSTATE_CONTINUING;
            //log_state_change(p[i].pid, PSTATE_STOPPED, PSTATE_CONTINUING, 0);
            //p_info(&p[i]);

            if (ptrace(PTRACE_CONT, p[i].pid, NULL, NULL) == -1) {
                perror("ptrace continue");
                log_error("error continuing with ptrace");
                return;
            }
            //printf("ssssaodfjhsaodhfa");

            p[i].state = PSTATE_RUNNING;
            log_state_change(p[i].pid, PSTATE_STOPPED, PSTATE_RUNNING, 0);
            p_info(&p[i]);
            break;
        }
    }

}


void show(char *arg) {
    int specific_id = -1; //default to showing all
    if (arg != NULL) {
        specific_id = atoi(arg);
        if (specific_id < 0 || specific_id >= num_processes || p[specific_id].state == PSTATE_NONE) {
            log_error("No such process deet ID");
            printf("No such process, deet ID %d.\n", specific_id);
            return;
        }
    }
    for (int i = 0; i < num_processes; i++) {
        p_info(&p[i]);
    }
}

void kill_process(int deet_id) {
    for (int i = 0; i < num_processes; i++) {
        if (p[i].deet_id == deet_id) {
            if (p[i].state != PSTATE_DEAD && p[i].state != PSTATE_KILLED) {
                if (kill(p[i].pid, SIGKILL) == -1) {
                    perror("error killing process");
                    log_error("error killing process");
                    return;
                }
                //PSTATE old_state = p[i].state;  // 保存旧状态
                kill(p[i].pid, SIGKILL);
                //p[i].state = PSTATE_KILLED;
                //log_state_change(p[i].pid, old_state, PSTATE_KILLED, 0);
                //p_info(&p[i]);
            } else{
                printf("process is already dead or being killed\n");
                log_error("process already dead or being killed");
            }
            return;
        }
    }
    printf("process in deet ID %d not found\n", deet_id);
    log_error("invalid deet ID");
}

PSTATE string_to_pstate(const char* state_str) {
    if (strcmp(state_str, "running") == 0) {
        return PSTATE_RUNNING;
    } else if (strcmp(state_str, "stopping") == 0) {
        return PSTATE_STOPPING;
    } else if(strcmp(state_str, "stopped") == 0) {
        return PSTATE_STOPPED;
    } else if (strcmp(state_str, "continuing") == 0) {
        return PSTATE_CONTINUING;
    } else if (strcmp(state_str, "killed") == 0) {
        return PSTATE_KILLED;
    } else if (strcmp(state_str, "dead") == 0) {
        return PSTATE_DEAD;
    } else {
        return PSTATE_NONE;
    }
}
void wait_command(int deet_id, const char* desired_state_str) {
    PSTATE desired_state = string_to_pstate(desired_state_str ? desired_state_str : "dead");
    int process_found = 0;

    for (int i = 0; i < num_processes; ++i) {
        if (p[i].deet_id == deet_id) {
            process_found = 1;

            while (1) {
                sigset_t mask, orig_mask;
                sigemptyset(&mask);
                //wait for SIGCHLD signals
                sigaddset(&mask, SIGCHLD);
                //block SIGCHLD
                if (sigprocmask(SIG_BLOCK, &mask, &orig_mask) < 0) {
                    perror("sigprocmask");
                    return;
                }
                if (p[i].state == desired_state || p[i].state == PSTATE_DEAD) {
                    break;
                }
                //Suspend, this is too hard to use
                //printf("gagasjafosaodfjhsaodhfa");
                sigsuspend(&orig_mask);
                //Restore
                if (sigprocmask(SIG_SETMASK, &orig_mask, NULL) < 0) {
                    perror("sigprocmask");
                    return;
                }
            }
            break;
        }
    }
    if (!process_found) {
        printf("Process, deet ID %d not found.\n", deet_id);
    }
}

void release_command(int deet_id) {
    for (int i = 0; i < num_processes; i++) {
        if (p[i].deet_id == deet_id) {
            if (ptrace(PTRACE_DETACH, p[i].pid, NULL, NULL) == -1) {
                perror("detaching failed");
                log_error("detaching failed");
                return;
            }
            p[i].is_traced = 0;
            if (p[i].state == PSTATE_STOPPED) {
                // Continue the process if it was stopped
                if (kill(p[i].pid, SIGCONT) == -1) {

                    perror("error continuing process after release");
                    log_error("error continuing process after release");
                    return;
                }
            }
            //printf("gagaga");
            log_state_change(p[i].pid, p[i].state, PSTATE_RUNNING, 0);
            p_info(&p[i]);
            return;
        }
    }
    printf("process, deet ID %d not found\n", deet_id);
    log_error("wrong deetID");
}

void peek_memory(int deet_id, unsigned long addr, int num_words) {
    for (int i = 0; i < num_processes; i++) {
        if (p[i].deet_id == deet_id) {
            if (p[i].state != PSTATE_STOPPED) {
                printf("no process stop\n");
                log_error("no process stop");
                return;
            }
            for (int j = 0; j < num_words; j++) {
                errno = 0;
                long data = ptrace(PTRACE_PEEKDATA, p[i].pid, addr + j * sizeof(long), NULL);
                if (errno != 0) {
                    perror("ptrace PEEKDATA error");
                    log_error("Ptrace peek error");
                    return;
                }
                printf("0x%lx: 0x%lx\n", addr + j * sizeof(long), data);
            }
            return;
        }
    }
    printf("process, deet ID %d not found\n", deet_id);
    log_error("invalid deet ID");
}

void poke_command(int deet_id, unsigned long address, unsigned long long value) {
    int found = 0;
    for (int i = 0; i < num_processes; i++) {
        if (p[i].deet_id == deet_id) {
            found = 1;
            if (p[i].is_traced) {
                if (ptrace(PTRACE_POKEDATA, p[i].pid, (void*)address, (void*)value) == -1) {
                    perror("ptrace poke");
                    log_error("Poking process memory failed");
                    return;
                }
                log_state_change(p[i].pid, p[i].state, p[i].state, 0);
                p_info(&p[i]);
            } else {
                printf("process, deet ID %d is not being traced\n", deet_id);
                log_error("process not being traced");
            }
            break;
        }
    }
    if (!found) {
        printf("process, deet ID %d not found\n", deet_id);
        log_error("invalid deet ID");
    }
}

//back trace
void bt(int deet_id, int max_length) {
    pid_t pid = p[deet_id].pid;
    struct user_regs_struct regs;

    if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) == -1) {
        perror("ptrace GETREGS");
        return;
    }
    unsigned long rbp = regs.rbp;
    unsigned long rip;
    for (int i = 0; i < max_length && rbp != 0 && rbp != (unsigned long)0x1; ++i) {
        rip = ptrace(PTRACE_PEEKDATA, pid, rbp + 8, NULL);
        if(errno != 0) {
            perror("ptrace PEEKDATA");
            break;
        }
        printf("%016lx\t%016lx\n", rbp, rip);
        rbp = ptrace(PTRACE_PEEKDATA, pid, rbp, NULL);
        if(errno != 0) {
            perror("ptrace PEEKDATA");
            break;
        }
    }
}

// run echo a b c
//args[0]=run;  args[1]=echo;  args[2]=a
//args[0]=release;  args[1]=1
void execute_command(char *command_line) {
    //sleep(1);
    command_line[strcspn(command_line, "\n")] = 0;

    //char command[256];
    char *args[256];
    int arg_count = 0;

    // strtok
    char *token = strtok(command_line, " ");
    while (token != NULL && arg_count < 256) {
        args[arg_count++] = token;
        token = strtok(NULL, " ");
    }
    args[arg_count] = NULL;
    //printf("sdfwerwer:'%s''%s''%s'\n", args[0],args[1],args[2]);
    //args[0]=run;  args[1]=echo;  args[2]=a

    if (arg_count <= 0) {
        log_error("No command entered");
        return;
    }
    //printf("aaaaaaaaaaaaaaaaaaaaaaaaaa%d\n",arg_count);
    //printf("Command: '%s'\n", args[0]);
    //sleep(1);
    if ((strcmp(args[0], "help\n") == 0)||(strcmp(args[0], "help") == 0)) {
        printf("Available commands:\n");
        printf("help -- Print this help message\n");
        printf("quit (<=0 args) -- Quit the program\n");
        printf("show (<=1 args) -- Show process info\n");
        printf("run (>=1 args) -- Start a process\n");
        printf("stop (1 args) -- Stop a running process\n");
        printf("cont (1 args) -- Continue a stopped process\n");
        printf("release (1 args) -- Stop tracing a process, allowing it to continue normally\n");
        printf("wait (1-2 args) -- Wait for a process to enter a specified state\n");
        printf("kill (1 args) -- Forcibly terminate a process\n");
        printf("peek (2-3 args) -- Read from the address space of a traced process\n");
        printf("poke (3 args) -- Write to the address space of a traced process\n");
        printf("bt (1-2 args) -- Show a stack trace for a traced process\n");

    } else if ((strcmp(args[0], "quit\n") == 0)||(strcmp(args[0], "quit") == 0)) {
        quit();
        sleep(3);
        log_shutdown();
        exit(EXIT_SUCCESS);

    } else if ((strcmp(args[0], "show\n") == 0)||(strcmp(args[0], "show") == 0)) {

        if (arg_count == 1) {
        show(NULL);
        } else {
        show(args[1]);
        }

    }else if ((strcmp(args[0], "run\n") == 0)||(strcmp(args[0], "run") == 0)){
        //printf("aaaaaaaaaaa%s\n", args[1]);
        run(args[1], args+1);

    } else if ((strcmp(args[0], "stop\n") == 0)||(strcmp(args[0], "stop") == 0)) {
        if (arg_count != 2) {
            printf("Usage: stop <deet_id>\n");
            log_error("stop: Incorrect number of argument");
        } else {
            int deet_id = atoi(args[1]);
            stop(deet_id);
        }

    } else if ((strcmp(args[0], "cont\n") == 0)||(strcmp(args[0], "cont") == 0)) {
        if (arg_count != 2) {
            printf("incorrect number of argument: cont <deet_id>\n");
            log_error("cont:Incorrect number of argument");
        } else {
            int deet_id = atoi(args[1]);
            continue1(deet_id);
        }
    } else if ((strcmp(args[0], "release\n") == 0)||(strcmp(args[0], "release") == 0)) {
        if (arg_count != 2) {
            printf("Usage: release <deet_id>\n");
            log_error("Incorrect number of arguments for release command");
        } else {
            int deet_id = atoi(args[1]);
            release_command(deet_id);
        }

    } else if ((strcmp(args[0], "wait\n") == 0)||(strcmp(args[0], "wait") == 0)) {
        if (arg_count < 2) {
            printf("Usage: wait <deet_id> [<state>]\n");
            log_error("Incorrect number of arguments for wait command");
        } else {
            int deet_id = atoi(args[1]);
            char* desired_state = (arg_count > 2) ? args[2] : NULL;
            wait_command(deet_id, desired_state);
        }

    } else if ((strcmp(args[0],"kill\n") == 0)||(strcmp(args[0], "kill") == 0)) {
        if (arg_count != 2) {
            printf("Usage: kill <deet_id>\n");
            log_error("Incorrect number of arguments for kill command");
        } else {
            int deet_id = atoi(args[1]);
            kill_process(deet_id);
        }

    } else if ((strcmp(args[0], "peek\n") == 0)||(strcmp(args[0], "peek") == 0)) {
        if (arg_count < 3 || arg_count > 4) {
            printf("Usage: peek <deet_id> <address> [num_words]\n");
            log_error("Incorrect number of arguments for peek command");
        } else {
            int deet_id = atoi(args[1]);
            unsigned long addr = strtoul(args[2], NULL, 16);
            int num_words = (arg_count == 4) ? atoi(args[3]) : 1;
            peek_memory(deet_id, addr, num_words);
        }
        //read memory from a traced process
    } else if ((strcmp(args[0], "poke\n") == 0)||(strcmp(args[0],"poke") == 0)) {
        if (arg_count != 4) {
            printf("Usage: poke <deet_id> <address> <value>\n");
            log_error("Incorrect number of arguments for poke command");
        } else {
            int deet_id = atoi(args[1]);
            unsigned long address = strtoul(args[2], NULL, 16);
            unsigned long long value = strtoull(args[3], NULL, 0);
            poke_command(deet_id, address, value);
        }

    } else if ((strcmp(args[0], "bt\n") == 0)||(strcmp(args[0], "bt") == 0)) {
        if (arg_count < 2) {
            printf("usage: bt <deet_id> [max_length]\n");
            log_error("incorrect number of arguments for bt command");
            return;
        }
        int deet_id = atoi(args[1]);
        int max_length = 10; // Default value

        if (arg_count > 2) {
            max_length = atoi(args[2]);
        }
        bt(deet_id, max_length);
    } else{
        log_error(args[0]);
        printf("?\n");
    }
}
