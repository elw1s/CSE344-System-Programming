#include "utils.h"


/* Buradaki bütün error içeren ve exit yapan iflere sigInt içindekileri koy, tabi bazıları null olabilir ona da bak. Aynısını serverda uygula */

char* directoryPath;
int portNumber;
char * serverAddress;
int client_socket;
char** parsedCommand;

struct FileNode* firstStartCurrentFiles = NULL;
struct FileNode* updated = NULL;
struct FileNode* currentFiles = NULL;

FILE* logFile;

void sigIntHandler(int signum) {
    fprintf(logFile, "SIGINT Received\n");
    free(directoryPath);
    free(serverAddress);
    fprintf(logFile, "Dynamically allocated char pointers are freed.\n");
    freeFileList(updated);
    freeFileList(currentFiles);
    freeFileList(firstStartCurrentFiles);
    fprintf(logFile, "Dynamically allocated struct FileNode* (Linked Lists) are freed.\n");
    free2DArray(parsedCommand);
    fprintf(logFile, "Dynamically allocated 2D Char array (parsedCommand) is freed.\n");
    close(client_socket);
    fprintf(logFile, "Client socket is closed\n");
    fclose(logFile);
    exit(EXIT_FAILURE);  
}


void send_message() {
    struct sockaddr_in server_address;
    char server_message[BUFFER_SIZE];
    char filePath[MAX_FILENAME];
    char client_message[BUFFER_SIZE];
    // Create socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        fprintf(logFile, "Error creating socket\n");
        exit(EXIT_FAILURE);
    }

    // Set server address
    server_address.sin_family = AF_INET;
    if(serverAddress != NULL){
        server_address.sin_addr.s_addr = inet_addr(serverAddress);
        fprintf(logFile, "Connecting to Server IP: %s.\n", serverAddress);
    }
    else{
        server_address.sin_addr.s_addr = inet_addr("127.0.0.1");
        fprintf(logFile, "Connecting to Server IP: 127.0.0.1.\n");
    }
    server_address.sin_port = htons(portNumber);

    // Connect to server
    if (connect(client_socket, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        fprintf(logFile, "Error connecting to server\n");
        exit(EXIT_FAILURE);
    }
    
    fprintf(logFile, "Getting all the files in folder for first synchronization...\n");
    getCurrentFilesFromDirectory(directoryPath, &firstStartCurrentFiles);
    modifyFilenames(firstStartCurrentFiles, directoryPath);

    if(firstStartCurrentFiles != NULL){
        fprintf(logFile, "There are files in folder...First synchronization is started\n");
        char * encodedMessage = encoder('0',firstStartCurrentFiles->isDirectory ? '1' : '0',firstStartCurrentFiles->filename);
        strcpy(client_message,encodedMessage);
        free(encodedMessage);
        struct FileNode * old = firstStartCurrentFiles;
        firstStartCurrentFiles = firstStartCurrentFiles->next;
        free(old); 
    }
    else{
        fprintf(logFile, "There are no files in folder...\n");
        char * encodedMessage = encoder('0','0',"");
        strcpy(client_message,encodedMessage);
        free(encodedMessage);
    }
    if (send(client_socket, client_message, BUFFER_SIZE, 0) <= 0) {
            fprintf(logFile, "Error sending message to server.\n");
            close(client_socket);
            exit(EXIT_FAILURE);
    }
    fprintf(logFile, "Message sent to the server.\n");
    int fd;
    char buffer[MAX_MESSAGE];
    
    getCurrentFilesFromDirectory(directoryPath,&currentFiles);
    modifyFilenames(currentFiles, directoryPath);
    int countUpdated = -1;
    struct timeval start_time, end_time;
    double lastCheckingTime;
    gettimeofday(&start_time, NULL);

    parsedCommand = (char**)malloc(sizeof(char*) * 3);
    parsedCommand[0] = (char*)malloc(sizeof(char) * 2);
    parsedCommand[1] = (char*)malloc(sizeof(char) * 2);
    parsedCommand[2] = (char*)malloc(sizeof(char) * 1023);
    while(1){
        memset(client_message, 0, BUFFER_SIZE);
        memset(server_message, 0, BUFFER_SIZE);
        int received;
        if ((received = recv(client_socket, server_message, BUFFER_SIZE, 0)) <= 0) {
            if (received == 0) {
                fprintf(logFile, "Server closed the connection.\n");
            } else {
                fprintf(logFile, "Error receiving message from server.\n");
            }
            free(serverAddress);
            freeFileList(updated);
            freeFileList(currentFiles);
            freeFileList(firstStartCurrentFiles);
            free2DArray(parsedCommand);
            free(directoryPath);
            fclose(logFile);
            close(client_socket);
            exit(EXIT_FAILURE);        
        }

        decoder(server_message, &parsedCommand);
        fprintf(logFile, "The message sent from server is decoded into parsedCommand array.\n");
        if(strcmp(parsedCommand[0],"0") == 0){
            if(firstStartCurrentFiles == NULL){
                if(updated != NULL){
                    if(updated->isDirectory){
                        if(updated->lastModificationTime == -1){
                            fprintf(logFile, "[SEND]: Remove directory\n");
                            char * encodedMessage = encoder('6','1',updated->filename);
                            strcpy(client_message,encodedMessage);
                            free(encodedMessage);
                        }
                        else{
                            fprintf(logFile, "[SEND]: Create directory\n");
                            char * encodedMessage = encoder('5','1',updated->filename);
                            strcpy(client_message,encodedMessage);
                            free(encodedMessage);
                        }
                    }
                    else{
                        if(updated->lastModificationTime == -1){
                            fprintf(logFile, "[SEND]: Remove file\n");
                            char * encodedMessage = encoder('4','0',updated->filename);
                            strcpy(client_message,encodedMessage);
                            free(encodedMessage);
                        }
                        else{
                            fprintf(logFile, "[SEND]: Open file\n");
                            char * encodedMessage = encoder('1','0',updated->filename);
                            strcpy(client_message,encodedMessage);
                            free(encodedMessage);
                        }
                    }                            
                    struct FileNode * old = updated;
                    updated = updated->next;  
                    free(old); 
                }
                else{
                    if(strcmp(parsedCommand[2], "") == 0){
                        gettimeofday(&end_time, NULL);
                        lastCheckingTime = (double)(end_time.tv_sec - start_time.tv_sec) +
                        (double)(end_time.tv_usec - start_time.tv_usec) / 1000000.0;
                        if(lastCheckingTime >= 5){
                            fprintf(logFile, "Checking the folder for updates...\n");
                            compareFiles(&updated,directoryPath,directoryPath,currentFiles, &countUpdated);
                            modifyFilenames(updated, directoryPath);
                            getCurrentFilesFromDirectory(directoryPath, &currentFiles);
                            modifyFilenames(currentFiles, directoryPath);
                            gettimeofday(&start_time, NULL);
                        }
                        if(updated != NULL){
                            if(updated->isDirectory){
                                if(updated->lastModificationTime == -1){
                                    fprintf(logFile, "[SEND]: Remove directory\n");
                                    char * encodedMessage = encoder('6','1',updated->filename);
                                    strcpy(client_message,encodedMessage);
                                    free(encodedMessage);
                                }
                                else{
                                    fprintf(logFile, "[SEND]: Create directory\n");
                                    char * encodedMessage = encoder('5','1',updated->filename);
                                    strcpy(client_message,encodedMessage);
                                    free(encodedMessage);
                                }
                            }
                            else{
                                if(updated->lastModificationTime == -1){
                                    fprintf(logFile, "[SEND]: Remove file\n");
                                    char * encodedMessage = encoder('4','0',updated->filename);
                                    strcpy(client_message,encodedMessage);
                                    free(encodedMessage);
                                }
                                else{
                                    fprintf(logFile, "[SEND]: Open file\n");
                                    char * encodedMessage = encoder('1','0',updated->filename);
                                    strcpy(client_message,encodedMessage);
                                    free(encodedMessage);
                                }
                            }  
                            struct FileNode * old = updated;
                            updated = updated->next;  
                            free(old);                          
                        }
                        else{
                            fprintf(logFile, "[SEND]: Check your folder for updates\n");
                            char * encodedMessage = encoder('0','0',"");
                            strcpy(client_message,encodedMessage);
                            free(encodedMessage);
                        }
                    }
                    else{
                        if(strcmp(parsedCommand[1],"0") == 0 && isFileAvailableInDirectory(directoryPath,parsedCommand[2])){
                            fprintf(logFile, "[SEND]: Check your folder for updates\n");
                            char * encodedMessage = encoder('0','0',"");
                            strcpy(client_message,encodedMessage);
                            free(encodedMessage);
                        }
                        else if(strcmp(parsedCommand[1],"1") == 0 && !isDirectoryAvailableInDirectory(directoryPath,parsedCommand[2])){
                            char combinedPath[MAX_FILENAME];
                            snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[2]);
                            if (mkdir(combinedPath, 0777) == -1) {
                                fprintf(logFile, "%s exists.\n", parsedCommand[2]);
                            }
                            fprintf(logFile, "[SEND]: Check your folder for updates\n");
                            char * encodedMessage = encoder('0','0',"");
                            strcpy(client_message,encodedMessage);
                            free(encodedMessage);                            
                        }
                        else{
                            if((strcmp(parsedCommand[1],"0") == 0)){
                                fprintf(logFile, "[SEND]: File does not exist in client directory\n");
                                char * encodedMessage = encoder('9','0',parsedCommand[2]);
                                strcpy(client_message,encodedMessage);
                                free(encodedMessage);
                            }
                            else{
                                fprintf(logFile, "[SEND]: Check your folder for updates\n");
                                char * encodedMessage = encoder('0','0',"");
                                strcpy(client_message,encodedMessage);
                                free(encodedMessage);
                            }
                        }
                    }
                }

            }
            else{                
                if(strcmp(parsedCommand[2], "") == 0){
                    fprintf(logFile, "[SEND]: Check the given file name if it is available in server directory\n");
                    char * encodedMessage = encoder('0',firstStartCurrentFiles->isDirectory ? '1' : '0',firstStartCurrentFiles->filename);
                    strcpy(client_message,encodedMessage);
                    free(encodedMessage);
                    struct FileNode * old = firstStartCurrentFiles;
                    firstStartCurrentFiles = firstStartCurrentFiles->next;  
                    free(old); 
                }
                else{
                    if(strcmp(parsedCommand[1],"0") == 0 && isFileAvailableInDirectory(directoryPath,parsedCommand[2])){
                        fprintf(logFile, "[SEND]: Check the given file name if it is available in server directory\n");
                        char * encodedMessage = encoder('0',firstStartCurrentFiles->isDirectory ? '1' : '0',firstStartCurrentFiles->filename);
                        strcpy(client_message,encodedMessage);
                        free(encodedMessage);
                        struct FileNode * old = firstStartCurrentFiles;
                        firstStartCurrentFiles = firstStartCurrentFiles->next;  
                        free(old); 
                    }
                    else if(strcmp(parsedCommand[1],"1") == 0 && !isDirectoryAvailableInDirectory(directoryPath,parsedCommand[2])){
                        char combinedPath[MAX_FILENAME];
                        snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[2]);
                        if (mkdir(combinedPath, 0777) == -1) {
                            fprintf(logFile, "%s exists.\n", parsedCommand[2]);
                        }
                        fprintf(logFile, "[SEND]: Check your folder for updates\n");
                        char * encodedMessage = encoder('0','0',"");
                        strcpy(client_message,encodedMessage);
                        free(encodedMessage);                        
                    }
                    else{
                        if(((strcmp(parsedCommand[1],"0") == 0))){
                            fprintf(logFile, "[SEND]: File does not exist in client directory\n");
                            char * encodedMessage = encoder('9','0',parsedCommand[2]);
                            strcpy(client_message,encodedMessage);
                            free(encodedMessage);
                        }
                        else{
                            fprintf(logFile, "[SEND]: Check your folder for updates\n");
                            char * encodedMessage = encoder('0','0',"");
                            strcpy(client_message,encodedMessage);
                            free(encodedMessage);
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
                    fprintf(logFile, "Failed to open %s file.\n", parsedCommand[2]);
                    fclose(logFile);
                    exit(-1);
            }
            fprintf(logFile, "%s is opened with O_WRONLY mode.\n", parsedCommand[2]);
            fprintf(logFile, "[SEND]: File is opened in client side. Server can send data\n");
            char * encodedMessage = encoder('8','0',parsedCommand[2]);
            strcpy(client_message,encodedMessage);
            free(encodedMessage);
            strcpy(filePath,parsedCommand[2]);            
        }
        else if(strcmp(parsedCommand[0], "2") == 0){
            ssize_t bytesWritten = write(fd, parsedCommand[2], strlen(parsedCommand[2]));
            fprintf(logFile, "Writing into the file...\n");
            if (bytesWritten == -1) {
                fprintf(logFile, "Error at writing to the file\n");
                close(fd);
                fclose(logFile);
                exit(-1);
            }
            fprintf(logFile, "[SEND]: The sent buffer is written into file. Server can send more data\n");
            char * encodedMessage = encoder('7','0',filePath);
            strcpy(client_message,encodedMessage);
            free(encodedMessage);
        }
        else if(strcmp(parsedCommand[0], "3") == 0){
            close(fd);
            getCurrentFilesFromDirectory(directoryPath, &currentFiles);
            modifyFilenames(currentFiles, directoryPath);
            fprintf(logFile, "Closing the %s file.\n", parsedCommand[2]);
            fprintf(logFile, "[SEND]: Check your folder for updates\n");
            char * encodedMessage = encoder('0','0',"");
            strcpy(client_message,encodedMessage);
            free(encodedMessage);
        }
        else if(strcmp(parsedCommand[0],"4") == 0){
            char combinedPath[MAX_FILENAME];
            snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[2]);
            remove(combinedPath);
            fprintf(logFile, "Removing the %s file.\n", parsedCommand[2]);
            fprintf(logFile, "[SEND]: Check your folder for updates\n");
            char * encodedMessage = encoder('0','0',"");
            strcpy(client_message,encodedMessage);
            free(encodedMessage);
        }
        else if(strcmp(parsedCommand[0],"5") == 0){
            char combinedPath[MAX_FILENAME];
            snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[2]);
            if (mkdir(combinedPath, 0777) == -1) {
                fprintf(logFile, "%s exists.\n", parsedCommand[2]);
            }
            fprintf(logFile, "[SEND]: Check your folder for updates\n");
            char * encodedMessage = encoder('0','0',"");
            strcpy(client_message,encodedMessage);
            free(encodedMessage);            
        }
        else if(strcmp(parsedCommand[0],"6") == 0){
            fprintf(logFile, "Removing the %s directory.\n", parsedCommand[2]);
            char combinedPath[MAX_FILENAME];
            snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[2]);
            removeFilesAndDirectories(combinedPath);
            fprintf(logFile, "[SEND]: Check your folder for updates\n");
            char * encodedMessage = encoder('0','0',"");
            strcpy(client_message,encodedMessage);
            free(encodedMessage);            
        }
        else if(strcmp(parsedCommand[0], "7") == 0 || strcmp(parsedCommand[0], "8") == 0){
            char combinedPath[MAX_FILENAME];
            snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[2]);
            if(strcmp(parsedCommand[0], "8") == 0) {
                fd = open(combinedPath,O_RDONLY, 0666);
                fprintf(logFile, "%s is opened with O_RDONLY mode. It is going to be read and send to the server.\n", parsedCommand[2]);
                if (fd == -1) {
                    perror("Failed to open file");
                    fclose(logFile);
                    exit(-1);
                }
            }

            memset(buffer, 0, sizeof(buffer));
            int bytesRead = read(fd, buffer, MAX_MESSAGE - 1);
            buffer[MAX_MESSAGE - 1] = '\0';
            if(bytesRead == -1){
                fprintf(logFile, "Error at reading %s file.\n", parsedCommand[2]);
                free(serverAddress);
                freeFileList(updated);
                freeFileList(currentFiles);
                freeFileList(firstStartCurrentFiles);
                free2DArray(parsedCommand);
                free(directoryPath);
                fclose(logFile);
                close(client_socket);
                exit(EXIT_FAILURE);
            }
            else if(bytesRead == 0){
                fprintf(logFile, "Reading the %s file is finished.\n", parsedCommand[2]);
                close(fd);
                fprintf(logFile, "[SEND]: Reached end of file. Server can close the file in its side.\n");
                char * encodedMessage = encoder('3','0',parsedCommand[2]);
                strcpy(client_message,encodedMessage);
                free(encodedMessage);
            }
            else{
                fprintf(logFile, "[SEND]: Buffer is read from file. Send it to server\n");
                char * encodedMessage = encoder('2','0',buffer);
                strcpy(client_message,encodedMessage);
                free(encodedMessage);
            }
        }
        else if(strcmp(parsedCommand[0],"9") == 0){
            fprintf(logFile, "Server does not have the file named %s.\n", parsedCommand[2]);
            fprintf(logFile, "[SEND]: Create file name on server side\n");
            char * encodedMessage = encoder('1','0',parsedCommand[2]);
            strcpy(client_message,encodedMessage);
            free(encodedMessage);
        }
        if (send(client_socket, client_message, BUFFER_SIZE, 0) <= 0) {
            fprintf(logFile, "Error sending message to server at send()\n");
            perror("Error sending message to server");
            free(serverAddress);
            freeFileList(updated);
            freeFileList(currentFiles);
            freeFileList(firstStartCurrentFiles);
            free2DArray(parsedCommand);
            free(directoryPath);
            fclose(logFile);
            close(client_socket);
            exit(EXIT_FAILURE);
        }
    }

    // Close the socket
    close(client_socket);
}

int main(int argc, char* argv[]) {

    if (argc < 3) {
        printf("Usage: %s <directory> <portNumber> <serverAddress (optional)>\n", argv[0]);
        return 1;
    }
    directoryPath = (char*) malloc(MAX_FILENAME);
    strcpy(directoryPath, argv[1]);
    printf("DirectoryPath = %s\n",directoryPath);
    if (mkdir(directoryPath, 0777) == -1) {
        printf("Directory exists\n");
    }
    struct sigaction sa_int;
    sa_int.sa_handler = sigIntHandler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &sa_int, NULL) == -1) {
        fprintf(logFile, "Error at sigaction()\n");
        fclose(logFile);
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    char combinedPath[MAX_FILENAME];
    snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, "clientLog");
    logFile = fopen(combinedPath, "w");


    portNumber = atoi(argv[2]);
    if(argc == 4){
        serverAddress = (char*)malloc(1024);
        strcpy(serverAddress, argv[3]);
    }
    else{
        serverAddress = NULL;
    }

    send_message();
    free(directoryPath);
    free(serverAddress);
    return 0;
}
