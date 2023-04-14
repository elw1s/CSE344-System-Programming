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

int redirectionHappend = 0;


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

int is_redirection_available(char * command){
    for(int i = 0; i < strlen(command); i++){
        if(command[i] == '<' || command[i] == '>')
            return 1;
    }
    return 0;
}

int is_redirection_output(char * command){
    for(int i = 0; i < strlen(command); i++){
        if(command[i] == '>')
            return 1;
    }
    return 0;
}

int is_redirection_input(char * command){
    for(int i = 0; i < strlen(command); i++){
        if(command[i] == '<')
            return 1;
    }
    return 0;
}


int shell(char* command, int pipes[2],int * write_fd, int * read_fd, FILE* logfile, int commandIndex, int totalProcesses){

    //FLAGS:    0 start
    //          1 middle
    //          2 end

    sigset_t blockMask, origMask;
    struct sigaction saIgnore, saOrigQuit, saOrigInt;
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

    /* If it is not the last process, then pipe should be created. For N process, N-1 pipe will be created */
    if(commandIndex + 1 < totalProcesses){
        pipe(pipes);
        *write_fd = pipes[1];
    }
    else{
        *write_fd = STDOUT_FILENO;
    }
    
    switch (childPid = fork()) {
        case -1: /* fork() failed */
                status = -1;
                break;
            case 0: /* Child */

                if(*read_fd != STDIN_FILENO){
                    dup2(pipes[0], STDIN_FILENO);
                    close(pipes[0]);
                }
                
                if(*write_fd != STDOUT_FILENO){
                    dup2(pipes[1], STDOUT_FILENO);
                    close(pipes[1]);
                }

                execl("/bin/sh", "sh", "-c", command, (char *) NULL);
                    perror("Execution of command failed\n");
                exit(1);
            default: 
                break;
    }

    savedErrno = errno;
    sigprocmask(SIG_SETMASK, &origMask, NULL);
    sigaction(SIGINT, &saOrigInt, NULL);
    sigaction(SIGQUIT, &saOrigQuit, NULL);
    errno = savedErrno;

    /* if(flag == 0 && pipes[pipeIndex][1] != STDOUT_FILENO){
        close(pipes[pipeIndex][1]);
    }
    else if(flag == 1 && pipes[pipeIndex-1][0] != STDIN_FILENO && pipes[pipeIndex][1] != STDOUT_FILENO){
        close(pipes[pipeIndex-1][0]);
        close(pipes[pipeIndex][1]);
    }
    else if(flag == 2 && pipes[pipeIndex - 1][0] != STDIN_FILENO){
        close(pipes[pipeIndex - 1][0]);
    }  */

    if(*read_fd != STDIN_FILENO){
        close(pipes[0]);
    }

    if(*write_fd != STDOUT_FILENO){
        close(pipes[1]);
    }

    if(commandIndex + 1 < totalProcesses){
        *read_fd = pipes[0];
        close(pipes[1]);
    }

    fprintf(logfile, "Command %d: %s, PID: %d\n", commandIndex, command, childPid);

    if (waitpid(childPid, &status, 0) == -1) {
        if (errno != EINTR) {
            status = -1;
        }
    }

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
        setbuf(stdin, NULL);
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

        /* int pipes[numberOfCommands - 1][2];
        for(int k = 0; k < numberOfCommands - 1; k++)
            if(pipe(pipes[k])== -1)
                perror("pipe error"); */

        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        char logfilename[256];
        
        snprintf(logfilename, sizeof(logfilename), "log_%04d-%02d-%02d_%02d-%02d-%02d.txt", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
        FILE *logfile = fopen(logfilename, "w");
        if (logfile == NULL) {
            perror("Error creating log file");
            exit(EXIT_FAILURE);
        }

        int pipes[2];
        int write_fd , read_fd = STDIN_FILENO;

        for(int i = 0; i < numberOfCommands; i++){
            /* int flag;
            if(i == 0) flag = 0;
            else if(i == numberOfCommands - 1) flag = 2;
            else flag = 1; */
            printf("write_fd = %d, read_fd = %d, i = %d\n", write_fd, read_fd, i);
            shell(commands[i], pipes, &write_fd, &read_fd, logfile, i, numberOfCommands);
        }       


        fprintf(logfile, "------------------------------\n");
        fclose(logfile);

    } 
    
    return 0;
}
