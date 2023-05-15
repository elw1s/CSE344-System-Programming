#include "biboConfiguration.h"

static char clientFifo[CLIENT_FIFO_NAME_LEN];
static void removeFifo(void) /* Invoked on exit to delete client FIFO */ 
{
    unlink(clientFifo);
}

enum RequestType getRequestType(const char *str) {
    if (strcmp(str, "Connect") == 0) {
        return CONNECT;
    } else if (strcmp(str, "tryConnect") == 0) {
        return TRY_CONNECT;
    } else if (strcmp(str, "help") == 0) {
        return HELP;
    } else if (strcmp(str, "list") == 0) {
        return LIST;
    } else if (strcmp(str, "readF") == 0) {
        return READF;
    } else if (strcmp(str, "writeT") == 0) {
        return WRITET;
    } else if (strcmp(str, "upload") == 0) {
        return UPLOAD;
    } else {
        return UNKNOWN;
    }
}

void printUsage(char *argv[]) {
    printf("Usage: %s <Connect/tryConnect> ServerPID [request...]\n", argv[0]);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    int serverFd, clientFd;
    struct request req;
    struct response resp;

    if (argc > 1 && strcmp(argv[1], "--help") == 0)
        printf("%s [seq-len...]\n", argv[0]);

    if (argc < 3) {
        printUsage(argv);
    }

    enum RequestType requestType = getRequestType(argv[1]);
    int serverPid = getInt(argv[2], "ServerPID");



    umask(0);
    /* So we get the permissions we want */
    snprintf(clientFifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE,(long) getpid());
    if (mkfifo(clientFifo, 0666) == -1)
        printf("mkfifo %s\n", clientFifo);
    if (atexit(removeFifo) != 0)
        printf("atexit");

    printf("serverPid = %d\n",serverPid);
    serverFd = open(SERVER_FIFO, O_WRONLY);
    printf("serverFd = %d\n", serverFd);
    if (serverFd == -1)
        printf("open %s", SERVER_FIFO);

    clientFd = open(clientFifo, O_RDONLY | O_NONBLOCK , 0666);
    if (clientFd == -1)
        printf("open %s", clientFifo);
    
    req.pid = getpid();
    req.type = CONNECT; // Add the request type to the request struct
    if (write(serverFd, &req, sizeof(struct request)) != sizeof(struct request)) {
        printf("Cannot connect to the server\n");
    }{
        printf("Connection is sent\n");
    }

   /*  // Handle request types
    switch (requestType) {
        case CONNECT:
            // Send connect request
            // ... connect code ...
            break;
        case TRY_CONNECT:
            // Send tryConnect request
            // ... tryConnect code ...
            break;
        case UNKNOWN:
        default:
            fprintf(stderr, "Unknown request: %s\n", argv[1]);
            exit(EXIT_FAILURE);
    } */

    while (1) {
        char command[BUF_SIZE];
        printf(">> Enter command: ");
        fgets(command, BUF_SIZE, stdin);
        command[strcspn(command, "\n")] = 0;  // Remove newline character

        enum RequestType requestType = getRequestType(command);
        req.type = requestType;
        // Process command accordingly
        switch (requestType) {
            case HELP:
                printf("Available commands are:\n");
                printf("help, list, readF <file> [<line #>], writeT <file> [<line #>] <string>, ");
                printf("upload <file>, download <file>, quit, killServer\n");
                break;
            case LIST:
                printf("LIST ICINDE\n");
                if (write(serverFd, &req, sizeof(struct request)) != sizeof(struct request)) {
                    printf("Can't write LIST request to server\n");
                }
                printf("WRITE YAPILDI\n");
                // Read the directory content from the server
                if (read(clientFd, &resp, sizeof(struct response)) != sizeof(struct response)) {
                    printf("Can't read directory content from server\n");
                }
                printf("Directory content:\n%s\n", resp.buffer);                        
                break;
            case QUIT:
                if (write(serverFd, &req, sizeof(struct request)) != sizeof(struct request)) {
                    printf("Cannot quit...\n");
                }
                break;
            case KILLSERVER:
                kill(serverPid, SIGTERM);
                printf(">> Kill signal sent to the server.\n");
                break;
            case READF:
                // ... readF code ...
                break;
            case WRITET:
                // ... writeT code ...
                break;
            case UPLOAD:
                // ... upload code ...
                break;
            case DOWNLOAD:
                // ... download code ...
                break;
            case UNKNOWN:
            default:
                fprintf(stderr, "Unknown command: %s\n", command);
        }
        
    }

    exit(EXIT_SUCCESS);
}