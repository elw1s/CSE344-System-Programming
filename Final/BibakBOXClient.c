#include "utils.h"

#define BUFFER_SIZE 2048

char* directoryPath;
int portNumber;
int serverAddress;

void send_message() {
    int client_socket;
    struct sockaddr_in server_address;
    char server_message[BUFFER_SIZE];
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

    if(firstStartCurrentFiles != NULL){
        strcpy(client_message, encoder('0',firstStartCurrentFiles->filename,""));
        firstStartCurrentFiles = firstStartCurrentFiles->next;
    }
    else{
        strcpy(client_message, encoder('0',"","")); // Set initial client_message to "kontrol"
    }
    if (send(client_socket, client_message, BUFFER_SIZE, 0) <= 0) {
            perror("Error sending message to server");
            exit(EXIT_FAILURE);
    }
    printf("Client socket = %d\n",client_socket);
    int countDosyaAc = 0;
    int okunanLine = 0;
    int fd;

    struct FileNode* updated = NULL;
    struct FileNode* currentFiles = getCurrentFilesFromDirectory(directoryPath); //Burada dosyadan okuma yap
    //struct FileNode* currentFiles = getCurrentFilesFromDirectory(directoryPath);
    int countUpdated = -1;

    char** parsedCommand;
    while(1){
        memset(client_message, 0, BUFFER_SIZE);
        memset(server_message, 0, BUFFER_SIZE);
        
        if (recv(client_socket, server_message, BUFFER_SIZE, 0) <= 0) {
            perror("Error receiving message from server");
            exit(EXIT_FAILURE);
        }
        parsedCommand = decoder(server_message);
        printf("-----------------\nCommand:%s\nFilename:%s\nMessage:%s\n-----------------\n",parsedCommand[0],parsedCommand[1],parsedCommand[2]);
        printf("Server Message = %s\n", server_message);
        /* If the message is CHECK DIRECTORY */
        if(strcmp(parsedCommand[0],"0") == 0){
            printf("Gelen komut: Klasörü veya dosyayı kontrol et.\n");
            if(firstStartCurrentFiles == NULL){
                printf("Senkronizasyon bitmiş.\n");
                //Burada senkronizasyon işlemini durdur.
                if(updated != NULL){
                    printf("Senkronizasyon bitmiş. Yeni güncellemeler var. Updated arrayinden bir file yollandı. (Dosya aç komutu ile)\n");
                    if(updated->newAddition){
                        strcpy(client_message,encoder('1',updated->filename,"NEW_ADDITION"));
                    }
                    else{
                        strcpy(client_message,encoder('1',updated->filename,""));
                    }
                    updated = updated->next;
                }
                else{
                    //Bütün dosyaları kontrol et
                    if(strcmp(parsedCommand[1], "") == 0){
                        updated = compareFiles(directoryPath,currentFiles, &countUpdated);
                        currentFiles = getCurrentFilesFromDirectory(directoryPath);
                        if(updated != NULL){
                            printf("Filename boş geldi. Bütün dosyalar kontrol edildi. Yeni bir degisiklik var.\n");
                            printf("Degisiklik olan dosya: %s, lastModificationTime: %ld\n",updated->filename,updated->lastModificationTime);
                            if(updated->newAddition){
                                strcpy(client_message,encoder('1',updated->filename,"NEW_ADDITION"));
                            }
                            else{
                                strcpy(client_message,encoder('1',updated->filename,""));
                            }
                            updated = updated->next;
                        }
                        else{
                            printf("Filename boş geldi. Bütün dosyalar kontrol edildi. Yeni bir degisiklik yok.\n");
                            strcpy(client_message,encoder('0',"",""));
                        }
                    }
                    //Sadece adı verilen dosyayı kontrol et
                    else{
                        if(isFileAvailableInDirectory(directoryPath,parsedCommand[1])){
                            //Eğer klasörde varsa kontrol et demeye devam et.
                            printf("Filename dolu geldi. Klasörde var. Kontrol et yollandı (Filenamesiz)\n");
                            strcpy(client_message,encoder('0',"",""));
                        }
                        else{
                            //Eğer klasörde yoksa, yok mesajı gönder
                            printf("Filename dolu geldi. Klasörde yok. File yok komutu yollandı\n");
                            strcpy(client_message,encoder('9',parsedCommand[1],""));
                        }
                    }
                }

            }
            else{
                //İlk senkronizasyona devam
                printf("Senkronizasyona devam...\n");

                if(strcmp(parsedCommand[1], "") == 0){
                    printf("Filename bos geldi. Senkronizasyona devam\n");
                    printf("Gönderilecek dosya: %s\n",firstStartCurrentFiles->filename);
                    printf("Encoder: %s\n",encoder('0',firstStartCurrentFiles->filename,""));
                    strcpy(client_message,encoder('0',firstStartCurrentFiles->filename,""));
                    firstStartCurrentFiles = firstStartCurrentFiles->next;      
                }
                //Sadece adı verilen dosyayı kontrol et
                else{
                    if(isFileAvailableInDirectory(directoryPath,parsedCommand[1])){
                        //Eğer klasörde varsa kontrol et demeye devam et.
                        printf("Filename dolu geldi. Klasörde var. Senkronizasyona devam\n");
                        printf("Gönderilecek dosya: %s\n",firstStartCurrentFiles->filename);
                        printf("Encoder: %s\n",encoder('0',firstStartCurrentFiles->filename,""));
                        strcpy(client_message,encoder('0',firstStartCurrentFiles->filename,""));
                        firstStartCurrentFiles = firstStartCurrentFiles->next;
                    }
                    else{
                        //Eğer klasörde yoksa, yok mesajı gönder
                        printf("Filename dolu geldi. Klasörde yok.\n");
                        strcpy(client_message,encoder('9',parsedCommand[1],""));
                    }
                }
            }
        }
        else if(strcmp(parsedCommand[0], "1") == 0){
            char* combinedPath = (char*)malloc(MAX_FILENAME * sizeof(char));
            snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[1]);
            if(strcmp(parsedCommand[2], "NEW_ADDITION") == 0 && isFileAvailableInDirectory(directoryPath,parsedCommand[1])){
                strcpy(client_message,encoder('0',"",""));
            }
            else{
                printf("FILE OPENED COMMAND FROM CLIENT IS SENT\n");
                fd = open(combinedPath,O_WRONLY | O_CREAT | O_TRUNC, 0666);
                strcpy(client_message,encoder('8',parsedCommand[1],""));
            }
            free(combinedPath);
        }
        //Eğer burada dosyadan okunan bytesWritten == 0 veyaaa EOF ulaşmışsa, dosya kapat komutu gönder
        else if(strcmp(parsedCommand[0], "2") == 0){
            ssize_t bytesWritten = write(fd, parsedCommand[2], strlen(parsedCommand[2]));
            if (bytesWritten == -1) {
                printf("ERRRORR\n");
                perror("Error writing to the file");
                close(fd);
                //Burada dosyaya yazılamadı de veya dosyayı kapat komutu gönder diger tarafta da dosyayı kapattır.
                //return 1;
            }
            printf("WRITTEN INTO THE FILE FROM CLIENT IS SENT\n");
            strcpy(client_message,encoder('7',parsedCommand[1],""));
        }
        else if(strcmp(parsedCommand[0], "3") == 0){
            close(fd);
            printf("File is closed in the client side\n");
            printf("CHECK COMMAND FROM CLIENT IS SENT\n");
            strcpy(client_message,encoder('0',"",""));
        }
        else if(strcmp(parsedCommand[0],"4") == 0){
            remove(parsedCommand[1]);
            printf("FILE IS REMOVED\n");
            //memset(server_message, 0, sizeof(server_message));
            //sprintf(server_message, "%d", fd);
            strcpy(client_message,encoder('0',"",""));
        }
        //Eğer burada dosyadan okunan bytesWritten == 0 veyaaa EOF ulaşmışsa, dosya kapat komutu gönder
        else if(strcmp(parsedCommand[0], "7") == 0 || strcmp(parsedCommand[0], "8") == 0){
        
            if(strcmp(parsedCommand[0], "8") == 0) {
                char* combinedPath = (char*)malloc(MAX_FILENAME * sizeof(char));
                snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[1]);
                fd = open(combinedPath,O_RDONLY, 0666);
                free(combinedPath);
            }

            char * buffer = (char*)malloc(1024);
            int bytesRead = read(fd, buffer, MAX_MESSAGE);
            printf("bytesRead = %d\n",bytesRead);
            printf("buffer: %s\n",buffer);
            if(bytesRead == -1){
                //Hata var
            }
            else if(bytesRead == 0){
                //Dosya okundu tamamen, dosya kapat komutu gönder
                close(fd);
                strcpy(client_message,encoder('3',parsedCommand[1],""));
            }
            else{
                //Dosyaya yazmaya devam
                printf("WRITE COMMAND FROM CLIENT IS SENT\n");
                strcpy(client_message, encoder('2',parsedCommand[1],buffer));
            }

            free(buffer);        
        }
        else if(strcmp(parsedCommand[0],"9") == 0){
            //remove(client_message);
            printf("%s is not in server. Send open file command\n",parsedCommand[1]);
            //memset(server_message, 0, sizeof(server_message));
            //sprintf(server_message, "%d", fd);
            strcpy(client_message,encoder('1',parsedCommand[1],""));
        }
/*         else{
            printf("Unknown command is received %s\n",server_message);
            strcpy(client_message,"0");
        } */

        //Add an array and get input from terminal, put into that array. After that print that array.
        /* printf("Enter a message: ");
        fgets(client_message, BUFFER_SIZE, stdin);
        client_message[strcspn(client_message, "\n")] = '\0'; // Remove trailing newline character
        printf("Gönderilen %s\n",client_message); */


        sleep(2); //SİL
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
    portNumber = atoi(argv[2]);
    serverAddress = atoi(argv[3]);

    send_message();
    return 0;
}
