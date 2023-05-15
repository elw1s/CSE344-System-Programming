#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <semaphore.h>

#define GN_GT_0 02


enum RequestType {
    CONNECT = 0,
    TRY_CONNECT,
    QUIT,
    KILLSERVER,
    HELP,
    LIST,
    READF,
    WRITET,
    UPLOAD,
    DOWNLOAD,
    UNKNOWN
};

#define SERVER_FIFO "/tmp/server.%ld"
#define CLIENT_FIFO_TEMPLATE "/tmp/client.%ld"
#define CLIENT_FIFO_NAME_LEN (sizeof(CLIENT_FIFO_TEMPLATE) + 20)
#define SERVER_FIFO_NAME_LEN (sizeof(SERVER_FIFO) + 20)

#define BUFFER_SIZE 4096 // Add this line


struct request {
    pid_t pid;
    int type;
    char buffer[BUFFER_SIZE];
    int clientId;
};

struct response {
    char buffer[BUFFER_SIZE];
    int clientId;
    int number;
    int connected;
};

#define BUF_SIZE 1024

int getInt(char *arg, const char *name) {
    char buf[BUF_SIZE];
    char *endptr;
    int res;

    snprintf(buf, BUF_SIZE, "%s", arg);

    errno = 0;
    res = strtol(buf, &endptr, 10);

    if (errno != 0 || *endptr != '\0' || endptr == buf) {
        fprintf(stderr, "Invalid argument: %s\n", name);
        exit(EXIT_FAILURE);
    }

    if (res <= 0) {
        fprintf(stderr, "%s must be greater than zero\n", name);
        exit(EXIT_FAILURE);
    }

    return res;
}
