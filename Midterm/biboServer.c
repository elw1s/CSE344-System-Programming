#include "biboConfiguration.h"

sem_t *emptySlotForClients;
sem_t *totalConnectedClients;
sem_t *connectionRequestSent;
Queue* queue;
int nextClientPid;
pid_t childPids[200];
int currentIndexChildPids = 0;
FILE * logFilePointer;

void sigChildHandler(int signum) {
    int savedErrno = errno;
    pid_t terminatedPid;
    
    while ((terminatedPid = waitpid(-1, NULL, WNOHANG)) > 0) {
        int foundIndex = -1;
        for (int i = 0; i < currentIndexChildPids; i++) {
            if (childPids[i] == terminatedPid) {
                foundIndex = i;
                break;
            }
        }
        if (foundIndex != -1) {
            // Shift elements to the left
            for (int i = foundIndex; i < currentIndexChildPids - 1; i++) {
                childPids[i] = childPids[i + 1];
            }
            currentIndexChildPids--;
            fprintf(logFilePointer, "Terminated child process with PID %d\n", terminatedPid);
        }
    }
    
    errno = savedErrno;
}

void sigIntHandler(int signum) {
    for (int i = 0; i < currentIndexChildPids; i++) {
        if (kill(childPids[i], SIGTERM) == -1) {
            fprintf(logFilePointer,"Failed to send kill signal to PID %d\n", childPids[i]);
        }
    }

    printf("bye...\n");
    fprintf(logFilePointer,"bye...");

    exit(EXIT_SUCCESS);
}

void server_send(const char *fifo_name, struct response *resp) {
    int fd = open(fifo_name, O_WRONLY);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    write(fd, resp, sizeof(struct response));
    fprintf(logFilePointer,"Server sent a response to the client...\n");
    close(fd);
}

void server_receive(const char *fifo_name, struct request *req) {
    int fd = open(fifo_name, O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    read(fd, req, sizeof(struct request));
    fprintf(logFilePointer,"Server received a request from a client...\n");
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
    char log_file[200];


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

    snprintf(log_file, sizeof(log_file), "%s/server%d.log",dirname,getpid());
    logFilePointer = fopen(log_file, "w");

    printf("LOGFILE %s created...\n",log_file);

    fprintf(logFilePointer, "Max clients = %d\n", maxClients);
    fprintf(logFilePointer, "Server PID: %d\n", getpid());
    
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

    struct sigaction sa_int;
    sa_int.sa_handler = sigIntHandler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &sa_int, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    sem_unlink("/emptySlotForClients");
    sem_unlink("/totalConnectedClients");
    sem_unlink("/connectionRequestSent");

    emptySlotForClients = sem_open("/emptySlotForClients", O_CREAT, 0644, maxClients);
    totalConnectedClients = sem_open("/totalConnectedClients", O_CREAT, 0644, 0);

    printf("Waiting for clients...\n");
    fprintf(logFilePointer,"Waiting for clients...\n");

    for(;;){
        server_receive(serverFifo, &req);
        int emptySlot;
        sem_getvalue(emptySlotForClients, &emptySlot);
        snprintf(clientFifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE, (long)req.pid);
        
        if(req.type == KILLSERVER){
            printf("kill signal from client%02d\n",req.clientId);
            fprintf(logFilePointer,"kill signal from client%02d\n",req.clientId);
            for (int i = 0; i < currentIndexChildPids; i++) {
                if (kill(childPids[i], SIGTERM) == -1) {
                    fprintf(logFilePointer,"Failed to send kill signal to PID %d\n", childPids[i]);
                    printf("Failed to send kill signal to PID %d\n", childPids[i]);
                }
            }
            printf("bye...\n");
            fprintf(logFilePointer,"bye...");
            exit(EXIT_SUCCESS);
        }

        if(isEmpty(queue)){
            nextClientPid = req.pid;
        }
        if(emptySlot == 0){
                if(!contains(queue,req.pid)){
                    fprintf(logFilePointer,"Connection request PID %ld... Queue FULL\n", (long)req.pid);
                    printf("Connection request PID %ld... Queue FULL\n", (long)req.pid);
                    add(queue,req.pid);
                    childPids[currentIndexChildPids++] = req.pid;
                }
                
                
                resp.connected = 0;
                resp.clientId = req.clientId;
                server_send(clientFifo, &resp);
                continue;
        }
        else if(emptySlot >= 1 && nextClientPid == req.pid){
            int removed = -1;
            if(!isEmpty(queue)){
                removed = dequeue(queue);
                if(!isEmpty(queue)){
                    nextClientPid = peek(queue);
                }
            }
            if(removed != -1){
                int foundIndex = -1;
                for (int i = 0; i < currentIndexChildPids; i++) {
                    if (childPids[i] == removed) {
                        foundIndex = i;
                        break;
                    }
                }
                if (foundIndex != -1) {
                    // Shift elements to the left
                    for (int i = foundIndex; i < currentIndexChildPids - 1; i++) {
                        childPids[i] = childPids[i + 1];
                    }
                    currentIndexChildPids--;
                }
            }
            
            resp.connected = 1;
            resp.clientId = req.clientId;
            strcpy(resp.directoryPath, dirname);
            server_send(clientFifo,&resp);
        }

        childPids[currentIndexChildPids++] = req.pid;
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
            fprintf(logFilePointer,">> Client PID %ld connected as client%02d\n", (long)req.pid, req.clientId);
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
                        int foundIndex = -1;
                        for (int i = 0; i < currentIndexChildPids; i++) {
                            if (childPids[i] == reqClient.pid) {
                                foundIndex = i;
                                break;
                            }
                        }
                        if (foundIndex != -1) {
                            // Shift elements to the left
                            for (int i = foundIndex; i < currentIndexChildPids - 1; i++) {
                                childPids[i] = childPids[i + 1];
                            }
                            currentIndexChildPids--;
                        }

                        fprintf(logFilePointer,">> client%02d disconnected..\n", reqClient.clientId);
                        printf(">> client%02d disconnected..\n", reqClient.clientId);
                        exit(EXIT_SUCCESS);
                    case LIST:
                        struct dirent *entry;
                        struct stat fileStat;
                        char contentBuf[BUFFER_SIZE] = "";

                        if (directory == NULL) {
                            fprintf(logFilePointer,"opendir\n");
                            exit(EXIT_FAILURE);
                        }
                        rewinddir(directory);
                        while ((entry = readdir(directory)) != NULL) {
                            char *filepath;
                            size_t len = strlen(dirname) + strlen(entry->d_name) + 2;
                            filepath = (char *)malloc(len * sizeof(char));
                            if (filepath == NULL) {
                                fprintf(logFilePointer,"Memory allocation failed.\n");
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
                        break;
                    case READF:                        
                        combined_size = strlen(dirname) + 1 + strlen(reqClient.filePath) + 1;
                        combined_path = malloc(combined_size * sizeof(char));
                        snprintf(combined_path, combined_size, "%s/%s", dirname, reqClient.filePath);
                        int readFd = open(combined_path,O_RDONLY, 0666);
                        ssize_t bytesRead;
                        respClient.readingFinished = 0;

                        while(1){
                            bytesRead = read(readFd, respClient.buffer, BUFFER_SIZE - 1);
                            if(bytesRead <= 0){
                                respClient.fd = bytesRead;
                                respClient.readingFinished = 1;
                            }
                            server_send(clientFifo, &respClient);
                            if(respClient.readingFinished == 1) break;
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
                            fprintf(logFilePointer,"Cannot open file for writeT..\n");
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
                            fprintf(logFilePointer,"Error at opening file for download..\n");
                            break;
                        } 
                        respClient.readingFinished = 0;
                        while(1){
                            sleep(1);
                            int status = read(fileDescriptor, respClient.buffer, sizeof(respClient.buffer) - 1);
                            if(status == -1){
                                fprintf(logFilePointer,"Error at downloading file..\n");
                                break;
                            }
                            else if(status == 0){
                                respClient.readingFinished = 1;
                            }
                            server_send(clientFifo, &respClient);
                            if(status == 0){
                                break;
                            } 
                            
                        }
                        memset(respClient.buffer, 0, sizeof(respClient.buffer));
                        close(fileDescriptor);
                        free(combined_path);
                        break;
                    case UPLOAD:
                        combined_size = strlen(dirname) + 1 + strlen(reqClient.filePath) + 1;
                        combined_path = malloc(combined_size * sizeof(char));
                        snprintf(combined_path, combined_size, "%s/%s", dirname, reqClient.filePath);
                        
                        FILE* uploadFile;
                        if(reqClient.uploadContinues == 1){
                            uploadFile = fopen(combined_path, "a");
                        }
                        else{
                            uploadFile = fopen(combined_path, "w");
                        }

                        if (uploadFile == NULL) {
                            fprintf(logFilePointer,"Failed to open the file for upload.\n");
                            break;
                        }
                        lock_file(uploadFile);
                        if (fwrite(reqClient.buffer, sizeof(char), strlen(reqClient.buffer), uploadFile) != strlen(reqClient.buffer)) {
                            fprintf(logFilePointer,"Error writing to the file.\n");
                            break;
                        }                     
                        unlock_file(uploadFile); 
                        
                        fclose(uploadFile);
                        free(combined_path);
                        break;
                }
            }
            exit(EXIT_SUCCESS);
        }
        else if(childPid == -1){

        }
        else{
            childPids[currentIndexChildPids++] = childPid;
        }

    }

    sem_close(emptySlotForClients);
    sem_unlink("/emptySlotForClients");
    sem_close(totalConnectedClients);
    sem_unlink("/totalConnectedClients");
    closedir(directory);
    free(queue);
    fclose(logFilePointer);

}

