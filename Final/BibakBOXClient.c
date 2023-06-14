#include "utils.h"


char* directoryPath;
int portNumber;
int serverAddress;

void send_message() {
    int client_socket;
    struct sockaddr_in server_address;
    char server_message[BUFFER_SIZE];
    char filePath[MAX_FILENAME];


    char client_message[BUFFER_SIZE];
    // Create socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Set server address
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_address.sin_port = htons(8888);

    // Connect to server
    if (connect(client_socket, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        perror("Error connecting to server");
        exit(EXIT_FAILURE);
    }
    
    struct FileNode* firstStartCurrentFiles = getCurrentFilesFromDirectory(directoryPath);
    modifyFilenames(firstStartCurrentFiles, directoryPath);

    if(firstStartCurrentFiles != NULL){
        strcpy(client_message, encoder('0',firstStartCurrentFiles->isDirectory ? '1' : '0',firstStartCurrentFiles->filename));
        firstStartCurrentFiles = firstStartCurrentFiles->next;
    }
    else{
        strcpy(client_message, encoder('0','0',"")); // Set initial client_message to "kontrol"
    }
    if (send(client_socket, client_message, BUFFER_SIZE, 0) <= 0) {
            perror("Error sending message to server");
            exit(EXIT_FAILURE);
    }
    printf("Client socket = %d\n",client_socket);
    int fd;
    char buffer[MAX_MESSAGE];
/*     struct flock fl;
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0; */

    struct FileNode* updated = NULL;
    struct FileNode* currentFiles = getCurrentFilesFromDirectory(directoryPath); //Burada dosyadan okuma yap
    modifyFilenames(currentFiles, directoryPath);
    int countUpdated = -1;
    struct timeval start_time, end_time;
    double lastCheckingTime;
    gettimeofday(&start_time, NULL);

    char** parsedCommand;
    while(1){
        memset(client_message, 0, BUFFER_SIZE);
        memset(server_message, 0, BUFFER_SIZE);
        int received;
        if ((received = recv(client_socket, server_message, BUFFER_SIZE, 0)) <= 0) {
            perror("Error receiving message from server");
            exit(EXIT_FAILURE);
        }
        parsedCommand = decoder(server_message);
        printf("-----------------\nCommand:%s\nIsdir:%s\nMessage:%s\n-----------------\n",parsedCommand[0],parsedCommand[1],parsedCommand[2]);
        printf("Server sent = %s\n", server_message);

        /* If the message is CHECK DIRECTORY */
        if(strcmp(parsedCommand[0],"0") == 0){
            if(firstStartCurrentFiles == NULL){
                if(updated != NULL){
                    if(updated->isDirectory){
                        if(updated->lastModificationTime == -1){
                            strcpy(client_message,encoder('6','1',updated->filename));
                        }
                        else{
                            strcpy(client_message,encoder('5','1',updated->filename));
                        }
                    }
                    else{
                        if(updated->lastModificationTime == -1){
                            strcpy(client_message,encoder('4','0',updated->filename));
                        }
                        else{
                            strcpy(client_message,encoder('1','0',updated->filename));
                        }
                    }                            
                    updated = updated->next;
                }
                else{
                    if(strcmp(parsedCommand[2], "") == 0){
                        gettimeofday(&end_time, NULL);
                        lastCheckingTime = (double)(end_time.tv_sec - start_time.tv_sec) +
                        (double)(end_time.tv_usec - start_time.tv_usec) / 1000000.0;
                        if(lastCheckingTime >= 5){
                            updated = compareFiles(directoryPath,directoryPath,currentFiles, &countUpdated);
                            modifyFilenames(updated, directoryPath);
                            currentFiles = getCurrentFilesFromDirectory(directoryPath);
                            modifyFilenames(currentFiles, directoryPath);
                            gettimeofday(&start_time, NULL);
                        }
                        if(updated != NULL){
                            if(updated->isDirectory){
                                if(updated->lastModificationTime == -1){
                                    strcpy(client_message,encoder('6','1',updated->filename));
                                }
                                else{
                                    strcpy(client_message,encoder('5','1',updated->filename));
                                }
                            }
                            else{
                                if(updated->lastModificationTime == -1){
                                    strcpy(client_message,encoder('4','0',updated->filename));
                                }
                                else{
                                    strcpy(client_message,encoder('1','0',updated->filename));
                                }
                            }                            
                            updated = updated->next;
                        }
                        else{
                            strcpy(client_message,encoder('0','0',""));
                        }
                    }
                    else{
                        if(strcmp(parsedCommand[1],"0") == 0 && isFileAvailableInDirectory(directoryPath,parsedCommand[2])){
                            strcpy(client_message,encoder('0','0',""));
                        }
                        else if(strcmp(parsedCommand[1],"1") == 0 && !isDirectoryAvailableInDirectory(directoryPath,parsedCommand[2])){
                            char combinedPath[MAX_FILENAME];
                            snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[2]);
                            if (mkdir(combinedPath, 0777) == -1) {
                                printf("Directory exists\n");
                            }
                            strcpy(client_message,encoder('0','0',""));
                            
                        }
                        else{
                            if((strcmp(parsedCommand[1],"0") == 0)){
                                strcpy(client_message,encoder('9','0',parsedCommand[2]));
                            }
                            else{
                                strcpy(client_message,encoder('0','0',""));
                            }
                        }
                    }
                }

            }
            else{
                //İlk senkronizasyona devam
                if(strcmp(parsedCommand[2], "") == 0){
                    strcpy(client_message,encoder('0',firstStartCurrentFiles->isDirectory ? '1' : '0',firstStartCurrentFiles->filename));
                    firstStartCurrentFiles = firstStartCurrentFiles->next;      
                }
                //Sadece adı verilen dosyayı kontrol et
                else{
                    if(strcmp(parsedCommand[1],"0") == 0 && isFileAvailableInDirectory(directoryPath,parsedCommand[2])){
                        strcpy(client_message,encoder('0',firstStartCurrentFiles->isDirectory ? '1' : '0',firstStartCurrentFiles->filename));
                        firstStartCurrentFiles = firstStartCurrentFiles->next;
                    }
                    else if(strcmp(parsedCommand[1],"1") == 0 && !isDirectoryAvailableInDirectory(directoryPath,parsedCommand[2])){
                        char combinedPath[MAX_FILENAME];
                        snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[2]);
                        if (mkdir(combinedPath, 0777) == -1) {
                            printf("Directory exists\n");
                        }
                        strcpy(client_message,encoder('0','0',""));
                        
                    }
                    else{
                        if(((strcmp(parsedCommand[1],"0") == 0))){
                            strcpy(client_message,encoder('9','0',parsedCommand[2]));
                        }
                        else{
                            strcpy(client_message,encoder('0','0',""));
                        }
                    }
                }
            }
        }
        else if(strcmp(parsedCommand[0], "1") == 0){
            char combinedPath[MAX_FILENAME];
            snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[2]);
            fd = open(combinedPath,O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (fd == -1) {
                    perror("Failed to open file");
                    exit(-1);
            }
            printf("%d opened\n",fd); 
/*             if(fcntl(fd,F_SETLKW, &fl) == -1){
                perror("Failed to acquire lock");
                close(fd);
            }
            printf("%d Locklandı\n",fd); 
 */            strcpy(client_message,encoder('8','0',parsedCommand[2]));
            strcpy(filePath,parsedCommand[2]);
            
        }
        else if(strcmp(parsedCommand[0], "2") == 0){
            ssize_t bytesWritten = write(fd, parsedCommand[2], strlen(parsedCommand[2]));

            if (bytesWritten == -1) {
                printf("ERRRORR\n");
                perror("Error writing to the file");
                close(fd);
                //return 1;
            }
           strcpy(client_message,encoder('7','0',filePath));
        }
        else if(strcmp(parsedCommand[0], "3") == 0){
/*             fl.l_type = F_UNLCK;
            if (fcntl(fd, F_SETLK, &fl) == -1) {
                perror("Failed to release lock");
                close(fd);
                exit(-1);
            }
 */            close(fd);
            currentFiles = getCurrentFilesFromDirectory(directoryPath);
            modifyFilenames(currentFiles, directoryPath);
            strcpy(client_message,encoder('0','0',""));
        }
        else if(strcmp(parsedCommand[0],"4") == 0){
            char combinedPath[MAX_FILENAME];
            snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[2]);
            remove(combinedPath);
            strcpy(client_message,encoder('0','0',""));
            
        }
        else if(strcmp(parsedCommand[0],"5") == 0){
            char combinedPath[MAX_FILENAME];
            snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[2]);
            if (mkdir(combinedPath, 0777) == -1) {
                printf("Directory exists\n");
            }
            strcpy(client_message,encoder('0','0',""));
            
        }
        else if(strcmp(parsedCommand[0],"6") == 0){
            char combinedPath[MAX_FILENAME];
            snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[2]);
            removeFilesAndDirectories(combinedPath);
            strcpy(client_message,encoder('0','0',""));
            
        }
        else if(strcmp(parsedCommand[0], "7") == 0 || strcmp(parsedCommand[0], "8") == 0){
            char combinedPath[MAX_FILENAME];
            snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[2]);
            if(strcmp(parsedCommand[0], "8") == 0) {
                fd = open(combinedPath,O_RDONLY, 0666);
                if (fd == -1) {
                    perror("Failed to open file");
                    exit(-1);
                }
                printf("%d opened\n",fd); 
/*                 if(fcntl(fd,F_SETLKW, &fl) == -1){
                    perror("Failed to acquire lock");
                    close(fd);
                    exit(-1);
               }                
               printf("%d Locklandı\n",fd); 
 */            }

            memset(buffer, 0, sizeof(buffer));
            int bytesRead = read(fd, buffer, MAX_MESSAGE - 1);
            buffer[MAX_MESSAGE - 1] = '\0';
           if(bytesRead == -1){
                //Hata var
            }
            else if(bytesRead == 0){
/*                 fl.l_type = F_UNLCK;
                if (fcntl(fd, F_SETLK, &fl) == -1) {
                    perror("Failed to release lock");
                    close(fd);
                    exit(-1);
                }    
 */                close(fd);
                strcpy(client_message,encoder('3','0',parsedCommand[2]));
            }
            else{
                strcpy(client_message, encoder('2','0',buffer));
            }
        }
        else if(strcmp(parsedCommand[0],"9") == 0){
           strcpy(client_message,encoder('1','0',parsedCommand[2]));
        }
        sleep(1); //SİL
        if (send(client_socket, client_message, BUFFER_SIZE, 0) <= 0) {
            perror("Error sending message to server");
            exit(EXIT_FAILURE);
        }
    }

    // Close the socket
    close(client_socket);
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        printf("Usage: %s <directory> <portNumber> <serverAddress>\n", argv[0]);
        return 1;
    }
    directoryPath = (char*) malloc(MAX_FILENAME);
    strcpy(directoryPath, argv[1]);
    printf("DirectoryPath = %s\n",directoryPath);
    if (mkdir(directoryPath, 0777) == -1) {
        printf("Directory exists\n");
    }

    portNumber = atoi(argv[2]);
    serverAddress = atoi(argv[3]);

    send_message();
    free(directoryPath);
    return 0;
}
