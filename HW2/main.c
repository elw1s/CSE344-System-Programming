#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/shm.h>

int shell(char * command){
    int status;
    pid_t childPid;
    switch (childPid = fork()) {
    case -1: /* Error */
        return -1;
    case 0: /* Child */
        int invalidCommand = execl("/bin/sh", "sh", "-c", command, (char *) NULL);

        _exit(127);
        /* Failed exec */
    default: /* Parent */
        if (waitpid(childPid, &status, 0) == -1)
        return -1;
        else
        return status;
    }
}

int main(int argc, char *argv[]) {


    shell("ls | grep main");
    return 0;

}