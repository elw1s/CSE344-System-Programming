#include "utils.h"

int server_socket;
char* directoryPath;
pthread_t  * threads;
int threadPoolSize;
int portNumber;
int currentThreadIndex = 0;
pthread_mutex_t mutexsigIntReceived = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexFileLock = PTHREAD_MUTEX_INITIALIZER;
volatile sig_atomic_t sigIntReceived = 0;


/* .jpg, .mp4 gibi dosyalarda çalışmıyor. Sebebi \0 sıkıntısı. Encoder tarafında \0 yapıldığı için problem yaşanıyor. */

void cleanThreadPool(){
    int i, j;
    for (i = 0; i < currentThreadIndex; i++) {
        void* exitStatus;
        if (pthread_tryjoin_np(threads[i], &exitStatus) == 0) {
            // Thread has exited, rearrange the array
            for (j = i; j < currentThreadIndex - 1; j++) {
                threads[j] = threads[j + 1];
            }
            currentThreadIndex--; // Decrement the count
            i--; // Decrement i since the array is shifted
        }
    }
}
void sigIntHandler(int signum) {
    pthread_mutex_lock(&mutexsigIntReceived);
    sigIntReceived = 1;
    cleanThreadPool();
    pthread_mutex_unlock(&mutexsigIntReceived);
    for (int i = 0; i < currentThreadIndex; i++) {
        pthread_join(threads[i], NULL);
        printf("Thread %d joined\n", i);
    }
    free(threads);
    free(directoryPath);
    pthread_mutex_destroy(&mutexsigIntReceived);
    pthread_mutex_destroy(&mutexFileLock);
    close(server_socket);
    exit(-1);
}
void* handle_client(void* arg) {
    int client_socket = *(int*)arg;
    char client_message[BUFFER_SIZE];
    char server_message[BUFFER_SIZE];
    int fd;
    char buffer[MAX_MESSAGE];
    printf("Client socket = %d\n",client_socket);
    char filePath[MAX_FILENAME];
    // Receive message from client

    int countUpdated = -1;
    struct FileNode* updated = NULL;
    struct FileNode* currentFiles = NULL;
    getCurrentFilesFromDirectory(directoryPath, &currentFiles);
    modifyFilenames(currentFiles, directoryPath);
    struct FileNode* firstStartCurrentFiles = NULL;
    getCurrentFilesFromDirectory(directoryPath, &firstStartCurrentFiles);
    modifyFilenames(firstStartCurrentFiles, directoryPath);
    struct timeval start_time, end_time;
    double lastCheckingTime;
    gettimeofday(&start_time, NULL);

    char** parsedCommand = (char**)malloc(sizeof(char*) * 3);
    parsedCommand[0] = (char*)malloc(sizeof(char) * 2);
    parsedCommand[1] = (char*)malloc(sizeof(char) * 2);
    parsedCommand[2] = (char*)malloc(sizeof(char) * 1023);
    while(1){
        memset(server_message, 0, BUFFER_SIZE);
        memset(client_message, 0, BUFFER_SIZE);
        // If SIGINT received, then terminate it.
        pthread_mutex_lock(&mutexsigIntReceived);
        //printf("sigIntReceived: %d\n",sigIntReceived);
        if(sigIntReceived){
            printf("SIGINT received\n");
            pthread_mutex_unlock(&mutexsigIntReceived);
            close(client_socket);
            free2DArray(parsedCommand);
            freeFileList(updated);
            freeFileList(currentFiles);
            freeFileList(firstStartCurrentFiles);
            pthread_exit(NULL);
        }
        pthread_mutex_unlock(&mutexsigIntReceived);
        if (recv(client_socket, client_message, BUFFER_SIZE, 0) <= 0) {
            perror("Client disconnected...\n");
            close(client_socket);
            free2DArray(parsedCommand);
            freeFileList(updated);
            freeFileList(currentFiles);
            freeFileList(firstStartCurrentFiles);
            pthread_exit(NULL);
        }
        decoder(client_message, &parsedCommand);
        printf("-----------------\nCommand:%s\nIsdir:%s\nMessage:%s\n-----------------\n",parsedCommand[0],parsedCommand[1],parsedCommand[2]);
        printf("Client sent = %s\n", client_message);

        /* If the message is CHECK DIRECTORY */
        if(strcmp(parsedCommand[0],"0") == 0){
             if(firstStartCurrentFiles == NULL){
                printf("Senkronizasyon yokkk\n");
                if(updated != NULL){
                    if(updated->isDirectory){
                        if(updated->lastModificationTime == -1){
                            char * encodedMessage = encoder('6','1',updated->filename);
                            strcpy(server_message,encodedMessage);
                            free(encodedMessage);
                        }
                        else{
                            char * encodedMessage = encoder('5','1',updated->filename);
                            strcpy(server_message,encodedMessage);
                            free(encodedMessage);
                        }
                    }
                    else{
                        if(updated->lastModificationTime == -1){
                            char * encodedMessage = encoder('4','0',updated->filename);
                            strcpy(server_message,encodedMessage);
                            free(encodedMessage);
                        }
                        else{
                            char * encodedMessage = encoder('1','0',updated->filename);
                            strcpy(server_message,encodedMessage);
                            free(encodedMessage);
                        }
                    }
                    struct FileNode * old = updated;
                    updated = updated->next;
                    free(old);

                }
                else{
                    //Bütün dosyaları kontrol et
                    if(strcmp(parsedCommand[2], "") == 0){
                        gettimeofday(&end_time, NULL);
                        lastCheckingTime = (double)(end_time.tv_sec - start_time.tv_sec) +
                        (double)(end_time.tv_usec - start_time.tv_usec) / 1000000.0;
                        if(lastCheckingTime >= 5){
                            /* updated =  */compareFiles(&updated,directoryPath,directoryPath,currentFiles, &countUpdated);
                            modifyFilenames(updated,directoryPath);
                            getCurrentFilesFromDirectory(directoryPath, &currentFiles);
                            modifyFilenames(currentFiles,directoryPath);
                            gettimeofday(&start_time, NULL);
                        }
                        if(updated != NULL){
                            if(updated->isDirectory){
                                if(updated->lastModificationTime == -1){
                                    char * encodedMessage = encoder('6','1',updated->filename);
                                    strcpy(server_message,encodedMessage);
                                    free(encodedMessage);
                                }
                                else{
                                    char * encodedMessage = encoder('5','1',updated->filename);
                                    strcpy(server_message,encodedMessage);
                                    free(encodedMessage);
                                }
                            }
                            else{
                                if(updated->lastModificationTime == -1){
                                    char * encodedMessage = encoder('4','0',updated->filename);
                                    strcpy(server_message,encodedMessage);
                                    free(encodedMessage);
                                    
                                }
                                else{
                                    char * encodedMessage = encoder('1','0',updated->filename);
                                    strcpy(server_message,encodedMessage);
                                    free(encodedMessage);
                                }
                            }
                            struct FileNode * old = updated;
                            updated = updated->next;
                            free(old);
                        }
                        else{
                            char * encodedMessage = encoder('0','0',"");
                            strcpy(server_message,encodedMessage);
                            free(encodedMessage);
                        }
                    }
                    else{
                        if(strcmp(parsedCommand[1],"0") == 0 && isFileAvailableInDirectory(directoryPath,parsedCommand[2])){
                            char * encodedMessage = encoder('0','0',"");
                            strcpy(server_message,encodedMessage);
                            free(encodedMessage);
                        }
                        else if(strcmp(parsedCommand[1],"1") == 0 && !isDirectoryAvailableInDirectory(directoryPath,parsedCommand[2])){
                            char combinedPath[MAX_FILENAME];
                            snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[2]);
                            if (mkdir(combinedPath, 0777) == -1) {
                                printf("Directory exists\n");
                            }
                            char * encodedMessage = encoder('0','0',"");
                            strcpy(server_message,encodedMessage);
                            free(encodedMessage);
                        }
                        else{
                            if((strcmp(parsedCommand[1],"0") == 0)){
                                char * encodedMessage = encoder('9','0',parsedCommand[2]);
                                strcpy(server_message,encodedMessage);
                                free(encodedMessage);
                            }
                            else{
                                char * encodedMessage = encoder('0','0',"");
                                strcpy(server_message,encodedMessage);
                                free(encodedMessage);
                            }
                        }
                    }
                }

            }
            else{
                //İlk senkronizasyona devam
                printf("Senkronizasyon var\n");
                if(strcmp(parsedCommand[2], "") == 0){
                    if(firstStartCurrentFiles->isDirectory){
                        char * encodedMessage = encoder('5','1',firstStartCurrentFiles->filename);
                        strcpy(server_message,encodedMessage);
                        free(encodedMessage);
                    }
                    else{
                        char * encodedMessage = encoder('1','0',firstStartCurrentFiles->filename);
                        strcpy(server_message,encodedMessage);
                        free(encodedMessage);
 
                    }
                    struct FileNode * old = firstStartCurrentFiles;
                    firstStartCurrentFiles = firstStartCurrentFiles->next;  
                    free(old);
                }
                else{
                    if(strcmp(parsedCommand[1],"0") == 0 && isFileAvailableInDirectory(directoryPath,parsedCommand[2])){
                        if(firstStartCurrentFiles->isDirectory){
                            char * encodedMessage = encoder('5','1',firstStartCurrentFiles->filename);
                            strcpy(server_message,encodedMessage); 
                            free(encodedMessage);
                        }
                        else{
                            char * encodedMessage = encoder('1','0',firstStartCurrentFiles->filename);
                            strcpy(server_message,encodedMessage); 
                            free(encodedMessage);
                        }
                        struct FileNode * old = firstStartCurrentFiles;
                        firstStartCurrentFiles = firstStartCurrentFiles->next;  
                        free(old);     
                    }
                    else if(strcmp(parsedCommand[1],"1") == 0 && !isDirectoryAvailableInDirectory(directoryPath,parsedCommand[2])){
                        char combinedPath[MAX_FILENAME];
                        snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[2]);
                        if (mkdir(combinedPath, 0777) == -1) {
                            printf("Directory exists\n");
                        }
                        char * encodedMessage = encoder('0','0',"");
                        strcpy(server_message,encodedMessage); 
                        free(encodedMessage);

                    }
                    else{ 
                        if((strcmp(parsedCommand[1],"0") == 0)){
                            char * encodedMessage = encoder('9','0',parsedCommand[2]);
                            strcpy(server_message,encodedMessage); 
                            free(encodedMessage);
                        }
                        else{
                            char * encodedMessage = encoder('0','0',"");
                            strcpy(server_message,encodedMessage); 
                            free(encodedMessage);
                        }
                    }                    
                }
            }
        }
        /* If the message is OPEN FILE */
        else if(strcmp(parsedCommand[0],"1") == 0){
            char combinedPath[MAX_FILENAME];
            snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[2]);
            fd = open(combinedPath,O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (fd == -1) {
                    perror("Failed to open file");
                    exit(-1);
            }
            pthread_mutex_lock(&mutexFileLock);
            char * encodedMessage = encoder('8','0',parsedCommand[2]);
            strcpy(server_message,encodedMessage); 
            free(encodedMessage);
            strcpy(filePath, parsedCommand[2]);
        }
        /* If the message is WRITE INTO FILE */
       else if(strcmp(parsedCommand[0],"2") == 0){
            ssize_t bytesWritten = write(fd, parsedCommand[2], strlen(parsedCommand[2]));
            if (bytesWritten == -1) {
                perror("Error writing to the file");
                close(fd);
                //return 1;
            }
            char * encodedMessage = encoder('7','0',filePath);
            strcpy(server_message,encodedMessage); 
            free(encodedMessage);
        }
        /* If the message is CLOSE FILE */
        else if(strcmp(parsedCommand[0],"3") == 0){
            close(fd);
            pthread_mutex_unlock(&mutexFileLock);
            getCurrentFilesFromDirectory(directoryPath, &currentFiles);
            modifyFilenames(currentFiles,directoryPath);
            char * encodedMessage = encoder('0','0',"");
            strcpy(server_message,encodedMessage); 
            free(encodedMessage);
        }
        /* If the message is "REMOVE FILE" */
        else if(strcmp(parsedCommand[0],"4") == 0){
            char combinedPath[MAX_FILENAME];
            snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[2]);
            remove(combinedPath);
            char * encodedMessage = encoder('0','0',"");
            strcpy(server_message,encodedMessage); 
            free(encodedMessage);
        }
        else if(strcmp(parsedCommand[0],"5") == 0){
            char combinedPath[MAX_FILENAME];
            snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[2]);
            if (mkdir(combinedPath, 0777) == -1) {
                printf("Directory exists\n");
            }
            char * encodedMessage = encoder('0','0',"");
            strcpy(server_message,encodedMessage); 
            free(encodedMessage);
        }
        else if(strcmp(parsedCommand[0],"6") == 0){
            char combinedPath[MAX_FILENAME];
            snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[2]);
            removeFilesAndDirectories(combinedPath);
            char * encodedMessage = encoder('0','0',"");
            strcpy(server_message,encodedMessage); 
            free(encodedMessage);
        }
        else if(strcmp(parsedCommand[0], "7") == 0 || strcmp(parsedCommand[0], "8") == 0){
            char combinedPath[MAX_FILENAME];
            snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[2]);
            if(strcmp(parsedCommand[0], "8") == 0){
                fd = open(combinedPath,O_RDONLY, 0666);
                if (fd == -1) {
                    perror("Failed to open file");
                    exit(-1);
                }
            }
            memset(buffer, 0, sizeof(buffer));
            int bytesRead = read(fd, buffer, MAX_MESSAGE - 1);
            buffer[MAX_MESSAGE - 1] = '\0';
            if(bytesRead == -1){
                //Hata var
            }
            else if(bytesRead == 0){
                close(fd);
                char * encodedMessage = encoder('3','0',parsedCommand[2]);
                strcpy(server_message,encodedMessage); 
                free(encodedMessage);
            }
            else{
                char * encodedMessage = encoder('2','0',buffer);
                strcpy(server_message,encodedMessage); 
                free(encodedMessage);
            }
        }
        else if(strcmp(parsedCommand[0],"9") == 0){
            char * encodedMessage = encoder('1','0',parsedCommand[2]);
            strcpy(server_message,encodedMessage); 
            free(encodedMessage);
        }
        // If SIGINT received, then terminate it.
        pthread_mutex_lock(&mutexsigIntReceived);
        if(sigIntReceived){
            pthread_mutex_unlock(&mutexsigIntReceived);
            printf("sigIntReceived in thread\n");
            close(client_socket);
            free2DArray(parsedCommand);
            freeFileList(updated);
            freeFileList(currentFiles);
            freeFileList(firstStartCurrentFiles);
            pthread_exit(NULL);
        }
        pthread_mutex_unlock(&mutexsigIntReceived);
        if ((send(client_socket, server_message, BUFFER_SIZE, 0)) <= 0) {
            perror("Error sending message to client");
            close(client_socket);
            free2DArray(parsedCommand);
            freeFileList(updated);
            freeFileList(currentFiles);
            freeFileList(firstStartCurrentFiles);
            pthread_exit(NULL);

        }
    }
    // Close the client socket
    printf("Client is disconnected..\n");
    close(client_socket);
    free(parsedCommand);
    pthread_exit(NULL);
}
int main(int argc, char* argv[]) {
    int client_socket;
    struct sockaddr_in server_address, client_address;
    socklen_t client_address_length;
    socklen_t server_address_length = sizeof(server_address);

    struct sigaction sa_int;
    sa_int.sa_handler = sigIntHandler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &sa_int, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }


    if (argc < 4) {
        printf("Usage: %s <directory> <threadPoolSize> <portnumber>\n", argv[0]);
        return 1;
    }
    directoryPath = (char*) malloc(MAX_FILENAME);
    strcpy(directoryPath, argv[1]);
    printf("DirectoryPath = %s\n",directoryPath);
    if (mkdir(directoryPath, 0777) == -1) {
        printf("Directory exists\n");
    }
    threadPoolSize = atoi(argv[2]);
    portNumber = atoi(argv[3]);

    threads = (pthread_t * ) malloc(sizeof(pthread_t) * threadPoolSize);

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }
    printf("Server socket: %d\n",server_socket);

    // Set server address
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(portNumber);

    // Bind socket to address
    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_socket, threadPoolSize) == -1) {
        perror("Error listening for connections");
        exit(EXIT_FAILURE);
    }  

    if (getsockname(server_socket, (struct sockaddr *)&server_address, &server_address_length) < 0) {
        perror("Failed to get socket address");
        return 1;
    }

    char ip_address[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &(server_address.sin_addr), ip_address, INET_ADDRSTRLEN) == NULL) {
        perror("Failed to convert IP address");
        return 1;
    }
        printf("*************************\n");
    printf("Server IP: %s\n", ip_address);
    printf("Port: %d\n", portNumber);
    printf("Server started. Listening for connections...\n");
    printf("*************************\n");
    

    while (1) {
        // Accept connection from client
        client_address_length = sizeof(client_address);
        client_socket = accept(server_socket, (struct sockaddr*)&client_address, &client_address_length);
        if (client_socket == -1) {
            perror("Error accepting connection");
            continue;
        }
        cleanThreadPool();
        if (currentThreadIndex >= threadPoolSize) {
            printf("Rejected client connection: Maximum thread pool size reached\n");
            close(client_socket); // Close the client socket to reject the connection
            continue;
        }
        
        printf("Client connected: %s:%d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
        // Start a new thread to handle the client
        if (pthread_create(&threads[currentThreadIndex], NULL, handle_client, &client_socket) != 0) {
            perror("Error creating thread");
            continue;
        }
        currentThreadIndex++;

        // Detach the thread to clean up resources automatically
        //pthread_detach(threads[currentThreadIndex - 1]);   
    }

    // Close the server socket
    close(server_socket);


    //Clean up all the pointers
    free(directoryPath);
    free(threads);
    pthread_mutex_destroy(&mutexsigIntReceived);
    pthread_mutex_destroy(&mutexFileLock);
    return 0;
}
