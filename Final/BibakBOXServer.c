#include "utils.h"

#define MAX_CLIENTS 5


char* directoryPath;
int threadPoolSize;
int portNumber;

int currentThreadIndex = 0;


void* handle_client(void* arg) {
    int client_socket = *(int*)arg;
    char client_message[BUFFER_SIZE];
    char server_message[BUFFER_SIZE];
    int fd;
    char buffer[MAX_MESSAGE];
    struct flock fl;
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;

    printf("Client socket = %d\n",client_socket);
    char filePath[MAX_FILENAME];
    // Receive message from client

    /* Demo Variables */
    int countUpdated = -1;
    struct FileNode* updated = NULL;
    struct FileNode* currentFiles = getCurrentFilesFromDirectory(directoryPath);
    printf("currentFiles\n");
    modifyFilenames(currentFiles, directoryPath);
    struct FileNode* firstStartCurrentFiles = getCurrentFilesFromDirectory(directoryPath);
    printf("firstStartCurrentFiles\n");
    modifyFilenames(firstStartCurrentFiles, directoryPath);
    struct timeval start_time, end_time;
    double lastCheckingTime;
    gettimeofday(&start_time, NULL);

    char** parsedCommand;
    while(1){
        memset(server_message, 0, BUFFER_SIZE);
        memset(client_message, 0, BUFFER_SIZE);
        if (recv(client_socket, client_message, BUFFER_SIZE, 0) <= 0) {
            perror("Error receiving message from client");
            close(client_socket);
            pthread_exit(NULL);
        }
        parsedCommand = decoder(client_message);
        printf("-----------------\nCommand:%s\nIsdir:%s\nMessage:%s\n-----------------\n",parsedCommand[0],parsedCommand[1],parsedCommand[2]);
        printf("Client sent = %s\n", client_message);

        /* If the message is CHECK DIRECTORY */
        if(strcmp(parsedCommand[0],"0") == 0){
             if(firstStartCurrentFiles == NULL){
                if(updated != NULL){
                    if(updated->isDirectory){
                        if(updated->lastModificationTime == -1){
                            strcpy(server_message,encoder('6','1',updated->filename));
                        }
                        else{
                            strcpy(server_message,encoder('5','1',updated->filename));
                        }
                    }
                    else{
                        if(updated->lastModificationTime == -1){
                            strcpy(server_message,encoder('4','0',updated->filename));
                        }
                        else{
                            strcpy(server_message,encoder('1','0',updated->filename));
                        }
                    }
                    updated = updated->next;
                }
                else{
                    //Bütün dosyaları kontrol et
                    if(strcmp(parsedCommand[2], "") == 0){
                        gettimeofday(&end_time, NULL);
                        lastCheckingTime = (double)(end_time.tv_sec - start_time.tv_sec) +
                        (double)(end_time.tv_usec - start_time.tv_usec) / 1000000.0;
                        if(lastCheckingTime >= 5){
                            updated = compareFiles(directoryPath,directoryPath,currentFiles, &countUpdated);
                            modifyFilenames(updated,directoryPath);
                            //printf("...........................................UPDATED\n");
                            currentFiles = getCurrentFilesFromDirectory(directoryPath);
                            modifyFilenames(currentFiles,directoryPath);
                            //printf("...........................................currentFiles\n");
                            gettimeofday(&start_time, NULL);
                        }
                        if(updated != NULL){
                            if(updated->isDirectory){
                                if(updated->lastModificationTime == -1){
                                    strcpy(server_message,encoder('6','1',updated->filename));
                                }
                                else{
                                    strcpy(server_message,encoder('5','1',updated->filename));
                                }
                            }
                            else{
                                if(updated->lastModificationTime == -1){
                                    strcpy(server_message,encoder('4','0',updated->filename));
                                }
                                else{
                                   strcpy(server_message,encoder('1','0',updated->filename));
                                }
                            }
                            updated = updated->next;
                        }
                        else{
                            strcpy(server_message,encoder('0','0',""));
                        }
                    }
                    else{
                        if(strcmp(parsedCommand[1],"0") == 0 && isFileAvailableInDirectory(directoryPath,parsedCommand[2])){
                            strcpy(server_message,encoder('0','0',""));
                        }
                        else if(strcmp(parsedCommand[1],"1") == 0 && !isDirectoryAvailableInDirectory(directoryPath,parsedCommand[2])){
                            char combinedPath[MAX_FILENAME];
                            snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[2]);
                            if (mkdir(combinedPath, 0777) == -1) {
                                printf("Directory exists\n");
                            }
                            strcpy(server_message,encoder('0','0',""));
                        }
                        else{
                            if((strcmp(parsedCommand[1],"0") == 0)){
                                strcpy(server_message,encoder('9','0',parsedCommand[2]));
                            }
                            else{
                                strcpy(server_message,encoder('0','0',""));
                            }
                        }
                    }
                }

            }
            else{
                //İlk senkronizasyona devam
                if(strcmp(parsedCommand[2], "") == 0){
                    if(firstStartCurrentFiles->isDirectory){
                        strcpy(server_message,encoder('5','1',firstStartCurrentFiles->filename)); 
                    }
                    else{
                        strcpy(server_message,encoder('1','0',firstStartCurrentFiles->filename)); 
                    }
                    firstStartCurrentFiles = firstStartCurrentFiles->next;      
                }
                else{
                    if(strcmp(parsedCommand[1],"0") == 0 && isFileAvailableInDirectory(directoryPath,parsedCommand[2])){
                        if(firstStartCurrentFiles->isDirectory){
                            strcpy(server_message,encoder('5','1',firstStartCurrentFiles->filename)); 
                        }
                        else{
                            strcpy(server_message,encoder('1','0',firstStartCurrentFiles->filename)); 
                        }
                        firstStartCurrentFiles = firstStartCurrentFiles->next;      

                    }
                    else if(strcmp(parsedCommand[1],"1") == 0 && !isDirectoryAvailableInDirectory(directoryPath,parsedCommand[2])){
                        char combinedPath[MAX_FILENAME];
                        snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[2]);
                        if (mkdir(combinedPath, 0777) == -1) {
                            printf("Directory exists\n");
                        }
                        strcpy(server_message,encoder('0','0',""));
                    }
                    else{ 
                        if((strcmp(parsedCommand[1],"0") == 0)){
                             strcpy(server_message,encoder('9','0',parsedCommand[2]));
                        }
                        else{
                            strcpy(server_message,encoder('0','0',""));
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
            printf("%d opened\n",fd); 
            fl.l_type = F_WRLCK;
            if(fcntl(fd,F_SETLKW, &fl) == -1){
                perror("Failed to acquire lock");
                close(fd);
                exit(-1);
            }  
            printf("%d Locklandı\n",fd);              
            strcpy(server_message,encoder('8','0',parsedCommand[2]));
            strcpy(filePath, parsedCommand[2]);
        }
        /* If the message is WRITE INTO FILE */
       else if(strcmp(parsedCommand[0],"2") == 0){
            ssize_t bytesWritten = write(fd, parsedCommand[2], strlen(parsedCommand[2]));
            if (bytesWritten == -1) {
                printf("ERRRORR\n");
                perror("Error writing to the file");
                close(fd);
                //return 1;
            }
            strcpy(server_message,encoder('7','0',filePath));
        }
        /* If the message is CLOSE FILE */
        else if(strcmp(parsedCommand[0],"3") == 0){
            fl.l_type = F_UNLCK;
            if (fcntl(fd, F_SETLK, &fl) == -1) {
                perror("Failed to release lock");
                close(fd);
                exit(-1);
            }            
            close(fd);
            currentFiles = getCurrentFilesFromDirectory(directoryPath);
            modifyFilenames(currentFiles,directoryPath);
            strcpy(server_message,encoder('0','0',""));
        }
        /* If the message is "REMOVE FILE" */
        else if(strcmp(parsedCommand[0],"4") == 0){
            char combinedPath[MAX_FILENAME];
            snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[2]);
            remove(combinedPath);
            strcpy(server_message,encoder('0','0',""));
        }
        else if(strcmp(parsedCommand[0],"5") == 0){
            char combinedPath[MAX_FILENAME];
            snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[2]);
            if (mkdir(combinedPath, 0777) == -1) {
                printf("Directory exists\n");
            }
            strcpy(server_message,encoder('0','0',""));
        }
        else if(strcmp(parsedCommand[0],"6") == 0){
            char combinedPath[MAX_FILENAME];
            snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[2]);
            removeFilesAndDirectories(combinedPath);
            strcpy(server_message,encoder('0','0',""));
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
                printf("%d opened\n",fd); 
                fl.l_type = F_RDLCK;
                if(fcntl(fd,F_SETLKW, &fl) == -1){
                    perror("Failed to acquire lock");
                    close(fd);
               }              
               printf("%d Locklandı\n",fd);   

            }
            memset(buffer, 0, sizeof(buffer));
            int bytesRead = read(fd, buffer, MAX_MESSAGE - 1);
            buffer[MAX_MESSAGE - 1] = '\0';
            if(bytesRead == -1){
                //Hata var
            }
            else if(bytesRead == 0){
                fl.l_type = F_UNLCK;
                if (fcntl(fd, F_SETLK, &fl) == -1) {
                    perror("Failed to release lock");
                    close(fd);
                    exit(-1);
                }            
                close(fd);
                strcpy(server_message,encoder('3','0',parsedCommand[2]));
            }
            else{
                strcpy(server_message, encoder('2','0',buffer));
            }
        }
        else if(strcmp(parsedCommand[0],"9") == 0){
            strcpy(server_message,encoder('1','0',parsedCommand[2]));
        }
        sleep(1); //SİL
        int sent;
        if ((sent = send(client_socket, server_message, BUFFER_SIZE, 0)) <= 0) {
            perror("Error sending message to client");
        }
    }
    // Close the client socket
    close(client_socket);
    pthread_exit(NULL);
}

int main(int argc, char* argv[]) {
    int server_socket, client_socket;
    struct sockaddr_in server_address, client_address;
    socklen_t client_address_length;
    pthread_t threads[MAX_CLIENTS];


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
    server_address.sin_port = htons(8888);

    // Bind socket to address
    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_socket, MAX_CLIENTS) == -1) {
        perror("Error listening for connections");
        exit(EXIT_FAILURE);
    }

    printf("Server started. Listening for connections...\n");

    while (1) {
        // Accept connection from client
        client_address_length = sizeof(client_address);
        client_socket = accept(server_socket, (struct sockaddr*)&client_address, &client_address_length);
        if (client_socket == -1) {
            perror("Error accepting connection");
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
        pthread_detach(threads[currentThreadIndex - 1]);
    }

    // Close the server socket
    close(server_socket);


    //Clean up all the pointers
    free(directoryPath);

    return 0;
}
