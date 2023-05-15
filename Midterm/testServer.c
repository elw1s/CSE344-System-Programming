#include "biboConfiguration.h"

sem_t *emptySlotForClients;
sem_t *totalConnectedClients;
sem_t *connectionRequestSent;
Queue* queue;
int nextClientPid;
FILE * readFp;

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
    size_t combined_size;
    char * combined_path;
    queue = createQueue();

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
        printf("BBBBBBBBBBB\n");
        /* int clientNumber;
        sem_getvalue(totalConnectedClients, &clientNumber); */
        int emptySlot;
        sem_getvalue(emptySlotForClients, &emptySlot);
        snprintf(clientFifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE, (long)req.pid);
        if(isEmpty(queue)){
            nextClientPid = req.pid;
        }
        if(emptySlot == 0){
            //if(req.type == CONNECT){
               /*  int currentSlotValue;
                while(1){
                    sem_getvalue(emptySlotForClients, &currentSlotValue);
                    if(currentSlotValue >= 1){
                        break;
                    }
                } */
                if(!contains(queue,req.pid)){
                    printf("Connection request PID %ld... Queue FULL\n", (long)req.pid);
                    add(queue,req.pid);
                }
                
                
                resp.connected = 0;
                resp.clientId = req.clientId;
                printf("AAAAAAAAAAAAAAAAAAAAAAA\n");
                server_send(clientFifo, &resp);
                continue;

          /*   }
            else if(req.type == TRY_CONNECT){
                resp.connected = 0;
                resp.clientId = req.clientId;
                server_send(clientFifo, &resp);
            } */
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
            printf("CCCCCCCCCCCCCCCCCC\n");
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
            printf(">> Client PID %ld connected as client%02d\n", (long)req.pid, req.clientId);

            while(1){
                server_receive(clientFifo, &req);
                switch(req.type){
                    case QUIT:
                        for(int i = 0; i < maxClients; i++){
                            if(clients[i] == req.pid){
                                clients[i] = -1;
                            }
                        }
                        printf(">> client%02d disconnected..\n", req.clientId);
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
                        resp.clientId = req.clientId;
                        server_send(clientFifo, &resp);
                        //memset(resp.buffer, 0, sizeof(resp.buffer));
                        break;
                    case READF:
                        printf("ReadF command...\n");
                        
                        combined_size = strlen(dirname) + 1 + strlen(req.filePath) + 1;

                        // Create a buffer to store the combined file path
                        combined_path = malloc(combined_size * sizeof(char));
                        // Create the combined file path using snprintf
                        snprintf(combined_path, combined_size, "%s/%s", dirname, req.filePath);
                        printf("combined_path =%s\n",combined_path);
                        if(req.readingStarted == 0){
                            printf("Opening readFp..\n");
                            readFp = fopen(combined_path,"r");
                            if(readFp == NULL){
                                printf("Could not open\n");
                            }
                            else
                                printf("readFp opened..\n");
                        }
                        else{
                            readFp = req.fp;
                        }

                        //add code here
                        //It should read a buffer sized 4095 characters 

                        ssize_t bytesRead;
                        resp.fp = readFp;
                        printf("Started reading..\n");

                        /* FREAD KISMINDA SONSUZ LOOPA GIRIYOR. TEK SEFERDE OKUYOR FAKAT EOF GELINCE PATLIYOR */

                        bytesRead = fread(resp.buffer, sizeof(char), BUFFER_SIZE - 1, readFp);
                        printf("Reading finished..\n");
                        resp.readingFinished = 0;
                        if (bytesRead < 0) {
                            resp.readingFinished = 1;
                        }
                        // Null-terminate the buffer
                        resp.buffer[bytesRead] = '\0';
                        printf("OKUNAN: %s\n",resp.buffer);
                        server_send(clientFifo, &resp);
                        memset(resp.buffer, 0, sizeof(resp.buffer));
                        fclose(readFp);
                        free(combined_path);
                        break;

                    case WRITET:
                        printf("Write komutu geldi...\n");
                        combined_size = strlen(dirname) + 1 + strlen(req.filePath) + 1;

                        // Create a buffer to store the combined file path
                        combined_path = malloc(combined_size * sizeof(char));
                        char * temp_combined_path = malloc(combined_size * sizeof(char));
                        // Create the combined file path using snprintf
                        snprintf(combined_path, combined_size, "%s/%s", dirname, req.filePath);
                        snprintf(temp_combined_path, combined_size, "%s/%s", dirname, "temp.txt");

                        printf("combinedPath = %s\n",combined_path);
                        FILE *file = fopen(combined_path, "r");
                        printf("file opened\n");
                        FILE *temp = fopen(temp_combined_path, "w");
                        printf("temp opened..\n");
                        if (!file) {
                            file = fopen(combined_path, "w");
                            if(file == NULL) {
                                printf("Error opening file!\n");
                                return 1;
                            }
                            fputs(req.buffer, file);
                        } else {
                            char temp_buffer[BUFFER_SIZE];
                            int count = 1;
                            printf("file is okayy\n");
                            while (fgets(temp_buffer, BUFFER_SIZE, file) != NULL) {
                                printf("%s\n",temp_buffer);
                                if (count == req.lineNumber) {
                                    fputs(req.buffer, temp);
                                    fputs("\n", temp); // Adding a newline character after the new content
                                }
                                fputs(temp_buffer, temp);
                                count++;
                            }
                            
                            // If lineNumber is -1 or larger than the total number of lines, write at the end of file
                            if(req.lineNumber == -1 || req.lineNumber >= count) {
                                fputs(req.buffer, temp);
                            }

                            fclose(file);
                            fclose(temp);
                            
                            // delete the original file and rename the temporary file to original file
                            remove(combined_path);
                            rename(temp_combined_path, combined_path);
                        }
                        
                        free(combined_path);
                        resp.writingFinished = 1;
                        server_send(clientFifo,&resp);

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

