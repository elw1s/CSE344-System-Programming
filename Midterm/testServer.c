#include "biboConfiguration.h"

sem_t *emptySlotForClients;
sem_t *totalConnectedClients;

void sigChildHandler(int signum) {
    int savedErrno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        //printf("Child process terminated...\n");
    }
    errno = savedErrno;
}

void server_send(const char *fifo_name, struct response *resp) {
    int fd = open(fifo_name, O_WRONLY);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    write(fd, resp, sizeof(struct response));
    close(fd);
}

void server_receive(const char *fifo_name, struct request *req) {
    int fd = open(fifo_name, O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    read(fd, req, sizeof(struct request));
    close(fd);
}

int main(int argc, char *argv[]){

    char serverFifo[SERVER_FIFO_NAME_LEN];
    int maxClients;
    int serverFd, clientFd;
    struct request req;
    struct response resp;
    char clientFifo[CLIENT_FIFO_NAME_LEN];

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <dirname> <maximum number of Clients>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *dirname = argv[1];
    maxClients = getInt(argv[2], "maximum number of Clients");

    pid_t clients[maxClients];


    snprintf(serverFifo, SERVER_FIFO_NAME_LEN, SERVER_FIFO,(long) getpid());

    umask(0);
    if (mkfifo(serverFifo, 0666) == -1 && errno != EEXIST)
        printf("mkfifo %s", serverFifo);

    printf("Server PID: %d\n", getpid());


    if(mkdir(dirname, 0777) == -1){
        printf("Directory exists\n");
    }
    DIR* directory = opendir(dirname);

    for(int i =0; i < maxClients; i++){
        clients[i] = -1;
    }

    struct sigaction sa_chld;
    sa_chld.sa_handler = sigChildHandler;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART;

    if (sigaction(SIGCHLD, &sa_chld, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    sem_unlink("/emptySlotForClients");
    sem_unlink("/totalConnectedClients");


    emptySlotForClients = sem_open("/emptySlotForClients", O_CREAT, 0644, maxClients);
    totalConnectedClients = sem_open("/totalConnectedClients", O_CREAT, 0644, 0);

    printf("Waiting for clients...\n");
    for(;;){
        server_receive(serverFifo, &req);
        int clientNumber;
        sem_getvalue(totalConnectedClients, &clientNumber);
        int emptySlot;
        sem_getvalue(emptySlotForClients, &emptySlot);
        snprintf(clientFifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE, (long)req.pid);
        if(emptySlot == 0){
            //if(req.type == CONNECT){
                printf("Connection request PID %ld... Queue FULL\n", (long)req.pid);
               /*  int currentSlotValue;
                while(1){
                    sem_getvalue(emptySlotForClients, &currentSlotValue);
                    if(currentSlotValue >= 1){
                        break;
                    }
                } */
                resp.connected = 0;
                resp.clientId = req.clientId;
                server_send(clientFifo, &resp);
                continue;

          /*   }
            else if(req.type == TRY_CONNECT){
                resp.connected = 0;
                resp.clientId = req.clientId;
                server_send(clientFifo, &resp);
            } */
        }
        else{
            resp.connected = 1;
            resp.clientId = req.clientId;
            server_send(clientFifo,&resp);
        }

        pid_t childPid = fork();

        if(childPid == 0){
            snprintf(clientFifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE, (long)req.pid);
            for(int i =0; i < maxClients; i++){
                if(clients[i] == -1){
                    clients[i] = req.pid;
                }
            }
            printf(">> Client PID %ld connected as client%02d\n", (long)req.pid, clientNumber);

            while(1){
                server_receive(clientFifo, &req);
                switch(req.type){
                    case CONNECT:
                        //Do nothing
                        break;
                    case QUIT:
                        for(int i = 0; i < maxClients; i++){
                            if(clients[i] == req.pid){
                                clients[i] = -1;
                            }
                        }
                        printf(">> client%02d disconnected..\n", clientNumber);
                        //emptySlotForClients--;
                        //sem_wait(&emptySlotForClients);
                        exit(EXIT_SUCCESS);
                    case LIST:
                        struct dirent *entry;
                        struct stat fileStat;
                        char contentBuf[BUFFER_SIZE] = "";

                        if (directory == NULL) {
                            perror("opendir");
                            exit(EXIT_FAILURE);
                        }
                        rewinddir(directory);
                        while ((entry = readdir(directory)) != NULL) {
                            char *filepath;
                            size_t len = strlen(dirname) + strlen(entry->d_name) + 2;
                            filepath = (char *)malloc(len * sizeof(char));
                            if (filepath == NULL) {
                                printf("Memory allocation failed.\n");
                                return 1;
                            }
                            snprintf(filepath, len, "%s/%s", dirname, entry->d_name);
                            if (stat(filepath, &fileStat) < 0) {
                                free(filepath);
                                continue;
                            }
                            if(S_ISREG(fileStat.st_mode)){
                                strcat(contentBuf, "\t");
                                strcat(contentBuf, entry->d_name);
                                strcat(contentBuf, "\n");
                            }
                            free(filepath);
                        }
                        strncpy(resp.buffer, contentBuf, BUFFER_SIZE - 1);
                        resp.buffer[BUFFER_SIZE - 1] = '\0';
                        resp.clientId = clientNumber;
                        server_send(clientFifo, &resp);
                        //memset(resp.buffer, 0, sizeof(resp.buffer));
                        break;
                }
            }
            exit(EXIT_SUCCESS);
        }
        else if(childPid == -1){

        }
        else{
            int returnStatus;    
            //waitpid(childPid, &returnStatus, 0);
            
        }


    }

    sem_close(emptySlotForClients);
    sem_unlink("/emptySlotForClients");
    sem_close(totalConnectedClients);
    sem_unlink("/totalConnectedClients");
    closedir(directory);


}

