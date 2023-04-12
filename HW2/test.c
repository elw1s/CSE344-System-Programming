#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define MAX_INPUT_SIZE 1024
#define MAX_ARGS 64
#define MAX_ARG_LENGTH 128
#define MAX_COMMANDS 64

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
    char* token = strtok(command, "|><");
    while (token != NULL) {
        commands[i] = token;
        token = strtok(NULL, "|><");
        i++;
    }
    commands[i] = NULL;

    for (int j = 0; j < i; j++) {
        int k = 0;
        token = strtok(commands[j], " ");
        while (token != NULL) {
            args[j][k] = token;
            token = strtok(NULL, " ");
            k++;
        }
        args[j][k] = NULL;
    }
}


int main(){

    char input[MAX_INPUT_SIZE];
    char* commands[MAX_COMMANDS];
    char* args[MAX_COMMANDS][MAX_ARGS];
    while (1) {
        printf("(ARTER)$:");
        fgets(input, MAX_INPUT_SIZE, stdin);
        input[strcspn(input, "\n")] = 0; // remove trailing newline

        if (strcmp(input, ":q") == 0) {
            break;
        }

        parse_command(input, commands, args);


        for (int i = 0; commands[i] != NULL; i++) {
            printf("Command %d: ", i);
            for (int j = 0; args[i][j] != NULL; j++) {
                printf("%s ", args[i][j]);
            }
            printf("\n");
            shell(commands[i]);

        }
    }


    //shell("cat main.c");
    return 0;
}
