#include "biboConfiguration.h"

sem_t *emptySlotForClients;
sem_t *totalConnectedClients;
sem_t *connectionRequestSent;
Queue* queue;
int nextClientPid;


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

int lock_file(FILE * filePointer) {
    int file = fileno(filePointer);
    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; // lock whole file
    lock.l_pid = getpid();

    fcntl(file, F_SETLKW, &lock);

    lock.l_type = F_RDLCK ;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; // lock whole file
    lock.l_pid = getpid();

    fcntl(file, F_SETLK, &lock);

    return 0;
}

int unlock_file(FILE * filePointer) {
    int file = fileno(filePointer);
    struct flock lock;
    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;  // unlock whole file
    lock.l_pid = getpid();

    fcntl(file, F_SETLKW, &lock);
    fcntl(file, F_SETLK, &lock);

    return 0;
}

int main(int argc, char *argv[]){

    char serverFifo[SERVER_FIFO_NAME_LEN];
    int maxClients;
    int serverFd, clientFd;
    char clientFifo[CLIENT_FIFO_NAME_LEN];
    size_t combined_size;
    char * combined_path;
    queue = createQueue();
    struct request req;
    struct response resp;


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
    sem_unlink("/connectionRequestSent");

    emptySlotForClients = sem_open("/emptySlotForClients", O_CREAT, 0644, maxClients);
    totalConnectedClients = sem_open("/totalConnectedClients", O_CREAT, 0644, 0);
    //connectionRequestSent = sem_open("/connectionRequestSent", O_CREAT, 0644, 1);  // Initialize the semaphore here

    printf("Waiting for clients...\n");
    for(;;){
        server_receive(serverFifo, &req);
        int emptySlot;
        sem_getvalue(emptySlotForClients, &emptySlot);
        snprintf(clientFifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE, (long)req.pid);
        if(isEmpty(queue)){
            nextClientPid = req.pid;
        }
        if(emptySlot == 0){
                if(!contains(queue,req.pid)){
                    printf("Connection request PID %ld... Queue FULL\n", (long)req.pid);
                    add(queue,req.pid);
                }
                
                
                resp.connected = 0;
                resp.clientId = req.clientId;
                server_send(clientFifo, &resp);
                continue;
        }
        else if(emptySlot >= 1 && nextClientPid == req.pid){
            if(!isEmpty(queue)){
                dequeue(queue);
                if(!isEmpty(queue)){
                    nextClientPid = peek(queue);
                }
            }
            resp.connected = 1;
            resp.clientId = req.clientId;
            strcpy(resp.directoryPath, dirname);
            server_send(clientFifo,&resp);
        }



        pid_t childPid = fork();
        

        if(childPid == 0){
            struct request reqClient;
            struct response respClient;

            snprintf(clientFifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE, (long)req.pid);
            for(int i =0; i < maxClients; i++){
                if(clients[i] == -1){
                    clients[i] = req.pid;
                }
            }
            printf(">> Client PID %ld connected as client%02d\n", (long)req.pid, req.clientId);

            while(1){
                server_receive(clientFifo, &reqClient);
                switch(reqClient.type){
                    case QUIT:
                        for(int i = 0; i < maxClients; i++){
                            if(clients[i] == reqClient.pid){
                                clients[i] = -1;
                            }
                        }
                        printf(">> client%02d disconnected..\n", reqClient.clientId);
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
                        strncpy(respClient.buffer, contentBuf, BUFFER_SIZE - 1);
                        respClient.buffer[BUFFER_SIZE - 1] = '\0';
                        respClient.clientId = reqClient.clientId;
                        server_send(clientFifo, &respClient);
                        //memset(respClient.buffer, 0, sizeof(respClient.buffer));
                        break;
                    case READF:
                        printf("ReadF command...\n");
                        
                        combined_size = strlen(dirname) + 1 + strlen(reqClient.filePath) + 1;
                        combined_path = malloc(combined_size * sizeof(char));
                        snprintf(combined_path, combined_size, "%s/%s", dirname, reqClient.filePath);
                        /* if(reqClient.readingStarted == 0){
                            printf("Opening readFp..\n");
                            readFd = open(combined_path,O_RDONLY | O_CREAT, 0666);
                            if(readFd == -1){
                                printf("Could not open\n");
                            }
                            else
                                printf("readFd opened..\n");
                        } */
                        int readFd = open(combined_path,O_RDONLY, 0666);

                        ssize_t bytesRead;
                        printf("Started reading..\n");
                        respClient.readingFinished = 0;

                        while(1){
                            printf("waiting to read\n");
                            bytesRead = read(readFd, respClient.buffer, BUFFER_SIZE - 1);
                            printf("%s\n",respClient.buffer);
                            printf("read\n");
                            if(bytesRead <= 0){
                                printf("bitti..\n");
                                respClient.fd = bytesRead;
                                respClient.readingFinished = 1;
                            }
                            server_send(clientFifo, &respClient);
                        }
                        close(readFd);
                        free(combined_path);
                        break;

                    case WRITET:
                        combined_size = strlen(dirname) + 1 + strlen(reqClient.filePath) + 1;
                        combined_path = malloc(combined_size * sizeof(char));
                        char * temp_combined_path = malloc(combined_size * sizeof(char));
                        snprintf(combined_path, combined_size, "%s/%s", dirname, reqClient.filePath);
                        snprintf(temp_combined_path, combined_size, "%s/%s", dirname, "temp.txt");

                            
                        FILE * file = fopen(combined_path, "r");
                        FILE * temp;
                        if(reqClient.lineNumber == -1){
                            temp = fopen(temp_combined_path, "a");
                        }
                        else{
                            temp = fopen(temp_combined_path, "w");
                        }

                        if(temp == NULL){
                            printf("Cannot open temp file\n");
                            break;
                        }
                        
                        char temp_buffer[BUFFER_SIZE];
                        int count = 1;

                        if(reqClient.lineNumber == -1){
                            lock_file(temp);
                            fputs(reqClient.buffer, temp);
                            unlock_file(temp);
                        }
                        else{
                            while (fgets(temp_buffer, BUFFER_SIZE, file) != NULL) {
                                lock_file(temp);
                                if (count == reqClient.lineNumber) {
                                    fputs(reqClient.buffer, temp);
                                    fputs("\n", temp);
                                }
                                fputs(temp_buffer, temp);
                                unlock_file(temp);
                                count++;
                            }
                        }

                        if(file != NULL){
                            fclose(file);
                        }

                        fclose(temp);


                        remove(combined_path);
                        rename(temp_combined_path, combined_path);

                        
                        free(combined_path);
                        respClient.writingFinished = 1;
                        server_send(clientFifo,&respClient);
                        break;
                    case DOWNLOAD:
                        combined_size = strlen(dirname) + 1 + strlen(reqClient.filePath) + 1;
                        combined_path = malloc(combined_size * sizeof(char));
                        snprintf(combined_path, combined_size, "%s/%s", dirname, reqClient.filePath);
                        int fileDescriptor = open(combined_path, O_RDONLY, 0666);
                        if(fileDescriptor == -1){
                            printf("Error at opening file\n");
                            break;
                        } 
                        respClient.readingFinished = 0;
                        while(1){
                            sleep(1);
                            int status = read(fileDescriptor, respClient.buffer, sizeof(respClient.buffer) - 1);
                            if(status == -1){
                                printf("Error at reading..\n");
                                break;
                            }
                            else if(status == 0){
                                respClient.readingFinished = 1;
                            }
                            server_send(clientFifo, &respClient);
                            if(status == 0){
                                printf("Exit...\n");
                                break;
                            } 
                            
                        }
                        memset(respClient.buffer, 0, sizeof(respClient.buffer));
                        close(fileDescriptor);
                        free(combined_path);
                        break;
                    case UPLOAD:
                        
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
    free(queue);

}

