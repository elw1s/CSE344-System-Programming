#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>

#define MAX_INPUT_SIZE 1024
#define MAX_ARGS 64
#define MAX_ARG_LENGTH 128
#define MAX_COMMANDS 64

char * parse_filename(char *command){

    char * delimiter = " "; 

    for(int i = 0; i < strlen(command); i++){
        if(command[i] == '<')
            delimiter = "<";
        else if(command[i] == '>')
            delimiter = ">";
    }
    if(strcmp(delimiter, " ") == 0) return NULL;

    //char * token = strtok(command, delimiter);
    strtok(command, delimiter);
    char * fileName = strtok(NULL, delimiter);

    while(*fileName == ' '){
        fileName++;
    }

    return fileName;
}

void parse_command(char* command, char** commands, char* args[MAX_COMMANDS][MAX_ARGS]) {
    int i = 0;
    char* token = strtok(command, "|\n");
    while (token != NULL) {
        commands[i] = token;
        token = strtok(NULL, "|\n");
        i++;
    }
    commands[i] = NULL;
}

int main(){

    char input[MAX_INPUT_SIZE];
    char* commands[MAX_COMMANDS];
    char* args[MAX_COMMANDS][MAX_ARGS];

    while (1) {

        printf("(ARTER)$:");
        fflush(stdout);
        fgets(input, MAX_INPUT_SIZE, stdin);
        input[strcspn(input, "\n")] = 0;

        if (strcmp(input, ":q") == 0) {
            break;
        }

        parse_command(input, commands, args);

        int numberOfCommands = 0;
        for (int i = 0; commands[i] != NULL; i++) {
            numberOfCommands ++;
        }

        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        char logfilename[256];
        
        snprintf(logfilename, sizeof(logfilename), "log_%04d-%02d-%02d_%02d-%02d-%02d.txt", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
        FILE *logfile = fopen(logfilename, "w");
        if (logfile == NULL) {
            perror("Error creating log file");
            exit(EXIT_FAILURE);
        }

        int pipes[numberOfCommands][2];
        int write_fd , read_fd = STDIN_FILENO;

        pid_t pids[numberOfCommands];
        sigset_t blockMask, origMask;
        struct sigaction saIgnore, saOrigQuit, saOrigInt;
        pid_t childPid;
        int status, savedErrno;

        for(int i = 0; i < numberOfCommands; i++){
            sigemptyset(&blockMask);
            /* Block SIGCHLD */
            sigaddset(&blockMask, SIGCHLD);
            sigprocmask(SIG_BLOCK, &blockMask, &origMask);

            saIgnore.sa_handler = SIG_IGN;
            /* Ignore SIGINT and SIGQUIT */
            saIgnore.sa_flags = 0;
            sigemptyset(&saIgnore.sa_mask);
            sigaction(SIGINT, &saIgnore, &saOrigInt);
            sigaction(SIGQUIT, &saIgnore, &saOrigQuit);

            /* If it is not the last process, then pipe should be created. For N process, N-1 pipe will be created */
            if(i + 1 < numberOfCommands){
                pipe(pipes[i]);
                write_fd = pipes[i][1];
            }
            else{
                write_fd = STDOUT_FILENO;
            }
            
            childPid = fork();

            pids[i] = childPid;

            switch (childPid) {
                case -1: /* fork() failed */
                        status = -1;
                        break;
                    case 0: /* Child */

                        if(read_fd != STDIN_FILENO){
                            dup2(pipes[i-1][0], STDIN_FILENO);
                            close(pipes[i-1][0]);
                        }
                        
                        if(write_fd != STDOUT_FILENO){
                            dup2(pipes[i][1], STDOUT_FILENO);
                            close(pipes[i][1]);
                        }

                        execl("/bin/sh", "sh", "-c", commands[i], (char *) NULL);
                        perror("Execution of command failed\n");
                        exit(1);
                        break;
                    default: 
                        break;
            }

            savedErrno = errno;
            sigprocmask(SIG_SETMASK, &origMask, NULL);
            sigaction(SIGINT, &saOrigInt, NULL);
            sigaction(SIGQUIT, &saOrigQuit, NULL);
            errno = savedErrno;

            if(read_fd != STDIN_FILENO){
                close(pipes[i-1][0]);
            }
            if(write_fd != STDOUT_FILENO){
                close(pipes[i][1]);
            }
            if(i + 1 < numberOfCommands){
                read_fd = pipes[i-1][0];
                close(pipes[i][1]);
            }

            fprintf(logfile, "Command %d: %s, PID: %d\n", i, commands[i], childPid);
        }

        for(int i = 0; i < numberOfCommands; i++){
            if (waitpid(pids[i], &status, 0) == -1) {
                if (errno != EINTR) {
                    status = -1;
                }
            }
        } 

        fprintf(logfile, "------------------------------\n");
        fclose(logfile);

    } 
    
    return 0;
}
