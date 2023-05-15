#include "biboConfiguration.h"

sem_t *emptySlotForClients;
sem_t *totalConnectedClients;


void client_send(const char *fifo_name, struct request *req) {
    int fd = open(fifo_name, O_WRONLY);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    write(fd, req, sizeof(struct request));
    close(fd);
}

void client_receive(const char *fifo_name, struct response *resp) {
    int fd = open(fifo_name, O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    read(fd, resp, sizeof(struct response));
    close(fd);
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
    } 
    else if (strcmp(str, "quit") == 0) {
        return QUIT;
    } else {
        return UNKNOWN;
    }
}

void printUsage(char *argv[]) {
    printf("Usage: %s <Connect/tryConnect> ServerPID [request...]\n", argv[0]);
    exit(EXIT_FAILURE);
}



int main(int argc, char *argv[]){

    char serverFifo[SERVER_FIFO_NAME_LEN];
    int clientNumber;
    int serverFd, clientFd;
    struct request req;
    struct response resp;
    char clientFifo[CLIENT_FIFO_NAME_LEN];
    int maxClients = 2;
    
    emptySlotForClients = sem_open("/emptySlotForClients", O_CREAT, 0644, maxClients);  // Initialize the semaphore here
    totalConnectedClients = sem_open("/totalConnectedClients", O_CREAT, 0644, 0);  // Initialize the semaphore here

    if (argc > 1 && strcmp(argv[1], "--help") == 0)
        printf("%s [seq-len...]\n", argv[0]);

    if (argc < 3) {
        printUsage(argv);
    }

    enum RequestType connectionType = getRequestType(argv[1]);
    int serverPid = getInt(argv[2], "ServerPID");
    snprintf(serverFifo, SERVER_FIFO_NAME_LEN, SERVER_FIFO,(long) serverPid);

    umask(0);
    snprintf(clientFifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE,(long) getpid());
    if (mkfifo(clientFifo, 0666) == -1)
        printf("mkfifo %s\n", clientFifo);


    req.pid = getpid();
    req.type = connectionType;
    sem_post(totalConnectedClients);
    sem_getvalue(totalConnectedClients, &clientNumber);
    req.clientId = clientNumber;
    client_send(serverFifo, &req);
    //if(connectionType == TRY_CONNECT){
    client_receive(clientFifo, &resp);
    if(connectionType == CONNECT && resp.connected == 0){
        int emptySlotAvailable;
        while(1){
            sem_getvalue(emptySlotForClients, &emptySlotAvailable);
            if(emptySlotAvailable >= 1){
                sleep(4);
                client_send(serverFifo, &req);
                printf("connection request has sent\n");
                client_receive(clientFifo, &resp);
                if(resp.connected == 1){
                    break;
                }
                printf("could not connect to the server...\n");
            }
        }
    }

    //}
    if((connectionType == TRY_CONNECT && resp.connected == 0) == 0){
        sem_wait(emptySlotForClients);

    /*     int value;
        sem_getvalue(emptySlotForClients, &value);
        printf("Semaphore değeri: %d\n", value);
    */
        while(1){

            char command[BUF_SIZE];
            printf(">> Enter command: ");
            fgets(command, BUF_SIZE, stdin);
            command[strcspn(command, "\n")] = 0;  // Remove newline character

            enum RequestType requestType = getRequestType(command);
            req.type = requestType;

            switch(requestType){
                case HELP:
                    printf("\tPossible list of client requests:\n");
                    printf("\tlist\nreadF <file> <line#>\nwriteT <file> <line#> <string>\nupload <file>\ndownload <file>\nquit\nkillServer\n");
                    //Todo: Buraya help list gibi yazılmasını da kabul ettirt.
                break;
                case LIST:
                    req.type = LIST;
                    req.clientId = clientNumber;
                    client_send(clientFifo, &req);
                    client_receive(clientFifo, &resp);
                    clientNumber = resp.clientId;
                    printf("%s\n",resp.buffer);
                break;
                case QUIT:
                    //quit
                    req.type = QUIT;
                    req.clientId = clientNumber;
                    //unlink(server_location);
                    client_send(clientFifo, &req);
                    sem_post(emptySlotForClients);
                    exit(EXIT_SUCCESS);
                break;
            }
        }
    }
    else{
        printf("Could not connect to the server... The queue is full.\n");
    }

    sem_close(emptySlotForClients);
    sem_unlink("/emptySlotForClients");
    sem_close(totalConnectedClients);
    sem_unlink("/totalConnectedClients");

    exit(0);
}