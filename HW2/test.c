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

int shell(const char *command)
{
    sigset_t blockMask, origMask;
    struct sigaction saIgnore, saOrigQuit, saOrigInt, saDefault;
    pid_t childPid;
    int status, savedErrno;
    
    
    if (command == NULL)
        return shell(":") == 0;

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
            saDefault.sa_handler = SIG_DFL;
            saDefault.sa_flags = 0;
            sigemptyset(&saDefault.sa_mask);
            if (saOrigInt.sa_handler != SIG_IGN)
                sigaction(SIGINT, &saDefault, NULL);
            if (saOrigQuit.sa_handler != SIG_IGN)
                sigaction(SIGQUIT, &saDefault, NULL);
            sigprocmask(SIG_SETMASK, &origMask, NULL);


            /* char *filename = parse_filename(command);
            if (filename != NULL) {
                int fd = open(filename, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
                if (fd == -1) {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                if (dup2(fd, STDOUT_FILENO) == -1) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
                close(fd);
            } */
            
            execl("/bin/sh", "sh", "-c", command, (char *) NULL);
            _exit(127);
            /* We could not exec the shell */
        default: /* Parent: wait for our child to terminate */
            while (waitpid(childPid, &status, 0) == -1) {
                if (errno != EINTR) {
                    /* Error other than EINTR */
                    status = -1;
                    break;
                    /* So exit loop */
                }
            }
            break;
    }

    /* Unblock SIGCHLD, restore dispositions of SIGINT and SIGQUIT */
    savedErrno = errno;
    sigprocmask(SIG_SETMASK, &origMask, NULL);
    sigaction(SIGINT, &saOrigInt, NULL);
    sigaction(SIGQUIT, &saOrigQuit, NULL);
    errno = savedErrno;
    /* The following may change 'errno' */
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


        for (int i = 0; commands[i] != NULL; i++) {
            shell(commands[i]);
        }
    } 
 

    //shell("ls > output.txt");
    return 0;
}
