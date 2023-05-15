#include "biboConfiguration.h"

// Global variables
pid_t *clientPids = NULL;
int clientsConnected = 0;
int maxClients = 0; // Add this line
FILE *logFile;


void sigHandler(int signum) {
    printf("\nReceived kill signal. Terminating child processes and closing log file...\n");
    for (int i = 0; i < clientsConnected; i++) {
        kill(clientPids[i], SIGTERM);
    }
    fclose(logFile);
    exit(EXIT_SUCCESS);
}

void sigChildHandler(int signum) {
    int savedErrno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        clientsConnected--;
    }
    errno = savedErrno;
}


int main(int argc, char *argv[])
{
    int serverFd, dummyFd, clientFd;
    char clientFifo[CLIENT_FIFO_NAME_LEN];
    struct request req;
    struct response resp;
    int seqNum = 0;


    if (argc != 3) {
        fprintf(stderr, "Usage: %s <dirname> <maximum number of Clients>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *dirname = argv[1];
    maxClients = getInt(argv[2], "maximum number of Clients");

    pid_t clients[maxClients];

    /* if (mkdir(SERVER_FIFO, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1 && errno != EEXIST) {
        perror("mkdir");
        exit(EXIT_FAILURE);
    } */

    /* char logFilePath[256];
    snprintf(logFilePath, sizeof(logFilePath), "%s/server_log.txt", SERVER_FIFO);
    logFile = fopen(logFilePath, "a");
    if (logFile == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    } */

    printf("Server PID: %d\n", getpid());
    printf("Waiting for clients...\n");

    /* Create well-known FIFO, and open it for reading */
    umask(0);
    /* So we get the permissions we want */
    if (mkfifo(SERVER_FIFO, 0666) == -1 && errno != EEXIST)
        printf("mkfifo %s", SERVER_FIFO);
    printf("fifo açıldı\n");
    serverFd = open(SERVER_FIFO, O_RDONLY | O_NONBLOCK,0666);
    printf("serverFd = %d\n",serverFd);
    if (serverFd == -1)
        printf("serverFd- open %s", SERVER_FIFO);
    
    /* Open an extra write descriptor, so that we never see EOF */
    /* dummyFd = open(SERVER_FIFO, O_WRONLY,0666);
    printf("dummyFd = %d\n",dummyFd);
    if (dummyFd == -1)
        printf("dummyFd- open %s\n", SERVER_FIFO); */
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        printf("signal");

    struct sigaction sa;
    sa.sa_handler = sigHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    struct sigaction sa_chld;
    sa_chld.sa_handler = sigChildHandler;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART;

    if (sigaction(SIGCHLD, &sa_chld, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    int count = 0;

    for (;;) {
        count ++;
        /* Read requests and send responses */
        if (read(serverFd, &req, sizeof(struct request)) != sizeof(struct request)) {
            //fprintf(stderr, "Error reading request; discarding\n");
            continue;
        }
    
        pid_t childPid = fork();
        if (childPid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (childPid == 0) {
            // Child process
            switch (req.type) {
                case CONNECT:
                     /* Open client FIFO (previously created by client) */
                    if (clientsConnected >= maxClients) {
                        printf("Connection request PID %ld... Queue FULL\n", (long)req.pid);
                        continue;
                    }

                    snprintf(clientFifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE, (long)req.pid);
                    printf("Before opening clientFd\n");
                    clientFd = open(clientFifo, O_WRONLY);
                    printf("clientFifo = %s\n",clientFifo);
                    printf("clientFd = %d\n",clientFd);
                    if (clientFd == -1) {
                        // Open failed, give up on client
                        perror("open");
                        continue;
                    }
                    for(int i =0; i < maxClients; i++){
                        if(clients[i] == NULL){
                            clients[i] = req.pid;
                        }
                    }
                    clientsConnected++;
                    /* Print client connection message */
                    printf(">> Client PID %ld connected as client%02d\n", (long)req.pid, clientsConnected);    
                    break;
                case TRY_CONNECT:
                    // Handle tryConnect request
                    // ... tryConnect code ...
                    break;
                case QUIT:
                    // Print log file
                    snprintf(clientFifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE, (long)req.pid);
                    
                    for(int i = 0; i < maxClients; i++){
                        if(clients[i] == req.pid){
                            clients[i] = NULL;
                            printf(">> client%02d disconnected..\n", clientsConnected);
                        }
                    }
                    
                case HELP:
                    // Handle help request
                    // ... help code ...
                    printf("PID = %d\n", req.pid);
                    break;
                case LIST:
                // Handle list request
                {
                    printf("LIST içindeyizz...\n");
                    DIR *dir;
                    struct dirent *entry;
                    char contentBuf[BUFFER_SIZE] = "";

                    dir = opendir(SERVER_FIFO);
                    if (dir == NULL) {
                        perror("opendir");
                        exit(EXIT_FAILURE);
                    }

                    while ((entry = readdir(dir)) != NULL) {
                        //if (entry->d_type == DT_REG) {
                            strcat(contentBuf, entry->d_name);
                            strcat(contentBuf, "\n");
                        //}
                    }

                    closedir(dir);

                    strncpy(req.buffer, contentBuf, BUFFER_SIZE - 1);
                    req.buffer[BUFFER_SIZE - 1] = '\0';
                    if (write(clientFd, &req, sizeof(struct request)) != sizeof(struct request)) {
                        fprintf(stderr, "Error writing to FIFO %s\n", clientFifo);
                    }
                }
                break;
                case READF:
                    // Handle readF request
                    // ... readF code ...
                    break;
                case WRITET:
                    // Handle writeT request
                    // ... writeT code ...
                    break;
                case UPLOAD:
                    // Handle upload request
                    // ... upload code ...
                    break;
                case UNKNOWN:
                default:
                    fprintf(stderr, "Unknown request type: %d\n", req.type);
                    exit(EXIT_FAILURE);
            }
            exit(EXIT_SUCCESS);

        }
        else{
            printf("PARENT PROCESS\n");
            clientPids = realloc(clientPids, (clientsConnected + 1) * sizeof(pid_t));
            if (clientPids == NULL) {
                perror("realloc");
                exit(EXIT_FAILURE);
            }
        }
        

    }
}