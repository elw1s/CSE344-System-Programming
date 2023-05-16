#include "biboConfiguration.h"

sem_t *emptySlotForClients;
sem_t *totalConnectedClients;
sem_t *connectionRequestSent;


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
    else if (strcmp(str, "download") == 0) {
        return DOWNLOAD;
    } 
    else if (strcmp(str, "quit") == 0) {
        return QUIT;
    } 
    else if (strcmp(str, "killServer") == 0) {
        return KILLSERVER;
    } 
    else {
        return UNKNOWN;
    }
}

void printUsage(char *argv[]) {
    printf("Usage: %s <Connect/tryConnect> ServerPID [request...]\n", argv[0]);
    exit(EXIT_FAILURE);
}

void printHelp(const char *command){
    if (strcmp(command, "list") == 0) {
        printf("\tlist\n\tsends a request to display the list of files in Servers directory.\n");
    } else if (strcmp(command, "readF") == 0) {
        printf("\treadF <file> <line #>\n\tdisplay the #th line of the <file>, returns with an error if <file> does not exists\n");
    } else if (strcmp(command, "writeT") == 0) {
        printf("\twriteT <file> <line #> <string>\n\trequest to write the content of “string” to the #th line the <file>, if the line # is not given writes to the end of file. If the file does not exists in Servers directory creates and edits the file at the same time\n");
    } else if (strcmp(command, "upload") == 0) {
        printf("\tupload <file>\n\tuploads the file from the current working directory of client to the Servers directory (beware of the cases no file in clients current working directory and file with the same name on Servers side)\n");
    } 
    else if (strcmp(command, "download") == 0) {
        printf("\tdownload <file>\n\trequest to receive <file> from Servers directory to client side\n");
    } 
    else if (strcmp(command, "quit") == 0) {
        printf("\tquit\n\tSend write request to Server side log file and quits\n");
    } 
    else if (strcmp(command, "killServer") == 0) {
        printf("\tkillServer\n\tSend write request to Server side log file and quits\n");
    }
    else if (strcmp(command, "help") == 0) {
        printf("\thelp\n\tdisplay the list of possible client requests\n");
    }
}

int main(int argc, char *argv[]){

    char serverFifo[SERVER_FIFO_NAME_LEN];
    int clientNumber;
    int serverFd, clientFd;
    struct request req;
    struct response resp;
    char clientFifo[CLIENT_FIFO_NAME_LEN];
    int maxClients = 2;
    size_t combined_size;
    char * combined_path;
    char currentDirectory[1024];
    char serverDirectory[1024];

    getcwd(currentDirectory, sizeof(currentDirectory));
    
    emptySlotForClients = sem_open("/emptySlotForClients", O_CREAT, 0644, maxClients);  // Initialize the semaphore here
    totalConnectedClients = sem_open("/totalConnectedClients", O_CREAT, 0644, 0);  // Initialize the semaphore here
    connectionRequestSent = sem_open("/connectionRequestSent", O_CREAT, 0644, 1);  // Initialize the semaphore here

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
            printf("boşluk araniyor...\n");
            if(emptySlotAvailable >= 1){
                //sleep(4);
                sem_wait(connectionRequestSent);
                printf("KITLENDI... servera istek yapiliyor...\n");
                int val;
                sem_getvalue(connectionRequestSent, &val);
                client_send(serverFifo, &req);
                printf("connection request has sent\n");
                client_receive(clientFifo, &resp);
                printf("ACILDI... server döndü\n");
                sem_post(connectionRequestSent);
                if(resp.connected == 1){
                    strcpy(serverDirectory, resp.directoryPath);
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

            char input[BUF_SIZE];
            char *commands[5];
            int commandIndex = 0;
            printf(">> Enter command: ");
            fgets(input, BUF_SIZE, stdin);
            input[strcspn(input, "\n")] = 0;  // Remove newline character

            //Parse the input
            char *token = strtok(input, " ");
            while (token != NULL) {
                commands[commandIndex] = token;
                commandIndex++;
                token = strtok(NULL, " ");
            }
        
            enum RequestType requestType = getRequestType(commands[0]);
            req.type = requestType;
            req.clientId = clientNumber;

            switch(requestType){
                case HELP:
                    if(commandIndex >= 2){
                        printHelp(commands[1]);
                    }
                    else{
                        printf("\tPossible list of client requests:\n");
                        printf("\tlist\n\treadF <file> <line#>\n\twriteT <file> <line#> <string>\n\tupload <file>\n\tdownload <file>\n\tquit\n\tkillServer\n");

                    }
                break;
                case LIST:
                    client_send(clientFifo, &req);
                    client_receive(clientFifo, &resp);
                    printf("%s\n",resp.buffer);
                break;
                case QUIT:
                    client_send(clientFifo, &req);
                    sem_post(emptySlotForClients);
                    exit(EXIT_SUCCESS);
                break;
                case READF:
                    if(commands[2] == NULL){
                        strcpy(req.filePath, commands[1]);
                        req.lineNumber = -1;
                        //req.readingStarted = 0;
                        resp.readingFinished = 0;
                        printf("before sending req\n");
                        client_send(clientFifo,&req);
                        printf("after sending req\n");
                        while(1){
                            client_receive(clientFifo, &resp);
                            /* if(resp.fd <= 0){
                                printf("Finished reading..\n");
                                break;
                            } */

                            if(resp.readingFinished == 1){
                                break;
                            }


                            //req.readingStarted = 1;
                            printf("%s\n",resp.buffer);
                            //memset(req.buffer, 0, sizeof(req.buffer));
                        }
                    }
                    else{

                    }
                break;
                case DOWNLOAD:
                    combined_size = strlen(currentDirectory) + 1 + strlen(commands[1]) + 1;
                    combined_path = malloc(combined_size * sizeof(char));
                    snprintf(combined_path, combined_size, "%s/%s", currentDirectory, commands[1]);

                    int fileDescriptor = open(combined_path, O_WRONLY | O_CREAT, 0644);
                    if (fileDescriptor == -1) {
                        printf("Failed to open the file.\n");
                        return 1;
                    }
                    if (lseek(fileDescriptor, 0, SEEK_END) != 0) {
                        if (ftruncate(fileDescriptor, 0) == -1) {
                            printf("Failed to clear the file.\n");
                            close(fileDescriptor);
                            return 1;
                        }
                        printf("The file is exist. It is replaced with the downloaded file.\n");
                    }
                    strcpy(req.filePath, commands[1]);
                    client_send(clientFifo, &req);
                    printf("\tDownload started...\n");
                    while(1){
                        client_receive(clientFifo, &resp);
                        if(resp.readingFinished == 1){
                            printf("Download finished...\n");
                            break;
                        }
                        if (write(fileDescriptor, resp.buffer, strlen(resp.buffer)) == -1){
                            printf("Error at writing to the file.\n");
                        }
                        //printf("%s",resp.buffer);  
                    }
                    free(combined_path);
                    close(fileDescriptor);
                break;

                case WRITET:
                    memset(req.buffer, 0, sizeof(req.buffer));
                    resp.writingFinished = 0;
                    if(isInteger(commands[2])){
                        //Line is given
                        req.lineNumber = atoi(commands[2]);
                        printf("line number = %d\n",req.lineNumber);
                        strcpy(req.buffer, commands[3]);
                    }  
                    else{
                        //Line is not given
                        req.lineNumber = -1;
                        strcpy(req.buffer, commands[2]);
                    }
                    strcpy(req.filePath, commands[1]);
                    printf("Gönderilen:%s\n",req.buffer);
                    client_send(clientFifo, &req);
                    client_receive(clientFifo, &resp);
                    if(resp.writingFinished){
                        printf("Writing finished.\n");
                    }
                    else{
                        printf("Could not write.\n");
                    }

                break;

                case KILLSERVER:
                    
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
    sem_close(connectionRequestSent);
    sem_unlink("/connectionRequestSent");

    exit(0);
}