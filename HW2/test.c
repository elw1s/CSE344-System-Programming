#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

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

int shell(char* command, int pipes[][2], int pipeIndex, int flag){

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

    return status;


}


int old(char* commands[MAX_COMMANDS], int numberOfCommands)
{
    sigset_t blockMask, origMask;
    struct sigaction saIgnore, saOrigQuit, saOrigInt, saDefault;
    pid_t childPids[numberOfCommands];
    int status, savedErrno;
    int i = 0;
    int pipes[numberOfCommands - 1][2];
    
    
    //Error checking ekle
    for(int k = 0; k < numberOfCommands - 1; k++)
        pipe(pipes[k]);
    //printf("Pipeları açtı\n");
    while(commands[i] != NULL){
        //printf("WHILE LOOP INSIDE\n");
        /* if (commands[i] == NULL)
            return shell(":") == 0; */

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

        switch (childPids[i] = fork()) {
            case -1: /* fork() failed */
                status = -1;
                break;
                /* Carry on to reset signal attributes */
            case 0: /* Child: exec command */
                if(i == 0){
                    printf("FIRST COMMAND\n");
                    dup2(pipes[i][1], STDOUT_FILENO);
                    printf("%d Before execl\n", i);
                    printf("The command =%s\n",commands[i]);
                    execl("/bin/sh", "sh", "-c", commands[i], (char *) NULL);
                    close(pipes[i][1]);
                    printf("%d After execl\n", i);

                }
                else if(i == numberOfCommands - 1){
                    printf("LAST COMMAND\n");
                    dup2(pipes[i-1][0], STDIN_FILENO);
                    printf("%d Before execl\n", i);
                    printf("The command =%s\n",commands[i]);
                    execl("/bin/sh", "sh", "-c", commands[i], (char *) NULL);
                    close(pipes[i-1][0]);
                    printf("%d After execl\n", i);
                }
                else{
                    printf("MIDDLE COMMANDS\n");
                    dup2(pipes[i-1][0], STDIN_FILENO);
                    dup2(pipes[i][1], STDOUT_FILENO);
                    printf("%d Before execl\n", i);
                    printf("The command =%s\n",commands[i]);
                    execl("/bin/sh", "sh", "-c", commands[i], (char *) NULL);
                    close(pipes[i-1][0]);
                    close(pipes[i][1]);
                    printf("%d After execl\n", i);
                }

/*                 printf("%d Before execl\n", i);
                printf("The command =%s\n",commands[i]);
                execl("/bin/sh", "sh", "-c", commands[i], (char *) NULL);
                printf("%d After execl\n", i);
 */                //_exit(127);
                exit(1);
                /* We could not exec the shell */
            default: /* Parent: wait for our child to terminate */
                break;
        }

        /* Unblock SIGCHLD, restore dispositions of SIGINT and SIGQUIT */
        savedErrno = errno;
        sigprocmask(SIG_SETMASK, &origMask, NULL);
        sigaction(SIGINT, &saOrigInt, NULL);
        sigaction(SIGQUIT, &saOrigQuit, NULL);
        errno = savedErrno;
        /* The following may change 'errno' */
        //return status;
        i++;
    }

/*     for(int k = 0; k < numberOfCommands - 1; k++){
        close(pipes[k][0]);
        close(pipes[k][1]);
    } */
    
    for(int k = 0; k < numberOfCommands; k++){
        if (waitpid(childPids[k], &status, 0) == -1) {
            if (errno != EINTR) {
                status = -1;
                break;
            }
        }
    }

    return status; //Remove this line
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

        for(int i = 0; i < numberOfCommands; i++){
            int flag;
            if(i == 0) flag = 0;
            else if(i == numberOfCommands - 1) flag = 2;
            else flag = 1;
            shell(commands[i], pipes, i, flag);
        }            

    } 
     return 0;
}
