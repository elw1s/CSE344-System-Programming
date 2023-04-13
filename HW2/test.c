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

    char * token = strtok(command, delimiter);
    char * fileName = strtok(NULL, delimiter);

    while(*fileName == ' '){
        fileName++;
    }

    return fileName;
    
}

int shell(char* command, int pipes[][2], int pipeIndex, int flag, FILE* logfile){

    //FLAGS:    0 start
    //          1 middle
    //          2 end

    sigset_t blockMask, origMask;
    struct sigaction saIgnore, saOrigQuit, saOrigInt, saDefault;
    pid_t childPid;
    int status, savedErrno;


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

    
    switch (childPid = fork()) {
        case -1: /* fork() failed */
                status = -1;
                break;
                /* Carry on to reset signal attributes */
            case 0: /* Child: exec command */
                if(flag == 0){
                dup2(pipes[pipeIndex][1], STDOUT_FILENO);
                }
                else if(flag == 1){
                    dup2(pipes[pipeIndex-1][0], STDIN_FILENO);
                    dup2(pipes[pipeIndex][1], STDOUT_FILENO);
                }
                else if(flag == 2){
                    dup2(pipes[pipeIndex - 1][0], STDIN_FILENO);
                }
                execl("/bin/sh", "sh", "-c", command, (char *) NULL);
                exit(1);
                /* We could not exec the shell */
            default: /* Parent: wait for our child to terminate */
                if (waitpid(childPid, &status, 0) == -1) {
                    if (errno != EINTR) {
                    status = -1;
                    break;
                }
        }
                break;
    }

    savedErrno = errno;
    sigprocmask(SIG_SETMASK, &origMask, NULL);
    sigaction(SIGINT, &saOrigInt, NULL);
    sigaction(SIGQUIT, &saOrigQuit, NULL);
    errno = savedErrno;

    if(flag == 0){
        close(pipes[pipeIndex][1]);
    }
    else if(flag == 1){
        close(pipes[pipeIndex-1][0]);
        close(pipes[pipeIndex][1]);
    }
    else if(flag == 2){
        close(pipes[pipeIndex - 1][0]);
    } 
    fprintf(logfile, "Command %d: %s, PID: %d\n", pipeIndex, command, childPid);
    return status;


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
        input[strcspn(input, "\n")] = 0; // remove trailing newline

        if (strcmp(input, ":q") == 0) {
            break;
        }

        parse_command(input, commands, args);

        int numberOfCommands = 0;
        for (int i = 0; commands[i] != NULL; i++) {
            numberOfCommands ++;
        }

        int pipes[numberOfCommands - 1][2];
        for(int k = 0; k < numberOfCommands - 1; k++)
            pipe(pipes[k]);

        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        char logfilename[256];
        
        snprintf(logfilename, sizeof(logfilename), "log_%04d-%02d-%02d_%02d-%02d-%02d.txt", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
        FILE *logfile = fopen(logfilename, "w");
        if (logfile == NULL) {
            perror("Error creating log file");
            exit(EXIT_FAILURE);
        }

        for(int i = 0; i < numberOfCommands; i++){
            int flag;
            if(i == 0) flag = 0;
            else if(i == numberOfCommands - 1) flag = 2;
            else flag = 1;
            shell(commands[i], pipes, i, flag, logfile);
        }       

        // log child process IDs and commands to file
        /* for(int i = 0; i < numberOfCommands; i++){
            fprintf(logfile, "Command %d: %s, PID: %d\n", i+1, commands[i], getpid());
        } */
        fprintf(logfile, "------------------------------\n");
        fclose(logfile);

    } 
    
    return 0;
}
