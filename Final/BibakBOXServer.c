#include "utils.h"

#define MAX_CLIENTS 5


/* 
Program çalışmadığı zaman eklenen / silinen dosyalar için bir yöntem bul. Belki bütün dosyaları baştan yazdırabilirsin?
Daha mantıklı bir yol: Program kapanırken bir dosyaya en son directorydeki güncel hallerini yaz. Bunu her 2 taraf için de yap. Ardından
program çalıştığında ve thread açıldığında, bu bilgileri bir arraye koy. Arraydeki değerler ile klasörde hali hazırda bulunan değerleri karşılaştır.
Değişiklik olanlar varsa bunları diger tarafa ilet.
 */

/* Check olayı: Klasör değerlerini oku, bir değişiklik var ise bunları arraye koy. Koyduğun arrayden eleman çek ve işlem yaptırt.
Arrayden elemanı azalt. Bunun için bir global index tut.
 */

/* İnner directorylerdeki fileların adreslerinin tam tutmaması. Sadece filename olduğundan dolayı klasörde bulunamayacak. Bu yüzden onların
    klasörlerini de hesaba kat. utils.h içerisinde traverseDirectory !!
 */

char* directoryPath;
int threadPoolSize;
int portNumber;

int currentThreadIndex = 0;


void* handle_client(void* arg) {
    int client_socket = *(int*)arg;
    char client_message[BUFFER_SIZE];
    char server_message[BUFFER_SIZE];
    int fd;
    printf("Client socket = %d\n",client_socket);
    char filePath[MAX_FILENAME];
    // Receive message from client

    /* Demo Variables */
    int countDosyaAc = 0;
    int okunanLine = 0;
    int countUpdated = -1;
    struct FileNode* updated = NULL;
    struct FileNode* currentFiles = getCurrentFilesFromDirectory(directoryPath);
    /* Aşağıdaki üç değişkenin görevi: Başlangıçtaki senkronizasyonu sağlamak */
    int firstStartNumberOfElements = countFiles(currentFiles);
    int firstSyncIndex = 0;
    struct FileNode* firstStartCurrentFiles = getCurrentFilesFromDirectory(directoryPath);
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
        printf("-----------------\nCommand:%s\nMessage:%s\n-----------------\n",parsedCommand[0],parsedCommand[1]);
        printf("Client Message = %s\n", client_message);

        /* If the message is CHECK DIRECTORY */
        if(strcmp(parsedCommand[0],"0") == 0){
            printf("Gelen komut: Klasörü veya dosyayı kontrol et.\n");
            if(firstStartCurrentFiles == NULL){
                printf("Senkronizasyon bitmiş.\n");
                //Burada senkronizasyon işlemini durdur.
                if(updated != NULL){
                    printf("countUpdated >= 0\n");
                    printf("Senkronizasyon bitmiş. Yeni güncellemeler var. Updated arrayinden bir file yollandı. (Dosya aç komutu ile)\n");
                    if(updated->isDirectory){
                        if(updated->lastModificationTime == -1){
                            strcpy(server_message,encoder('6',updated->filename));
                        }
                        else{
                            strcpy(server_message,encoder('5',updated->filename));
                        }
                    }
                    else{
                        if(updated->lastModificationTime == -1){
                            strcpy(server_message,encoder('4',updated->filename));
                        }
                        else{
                            printf("Eski dosya\n");
                            strcpy(server_message,encoder('1',updated->filename));
                        }
                    }
                    updated = updated->next;
                }
                else{
                    //Bütün dosyaları kontrol et
                    if(strcmp(parsedCommand[1], "") == 0){
                        updated = compareFiles(directoryPath,currentFiles, &countUpdated);
                        currentFiles = getCurrentFilesFromDirectory(directoryPath);
                        if(updated != NULL){
                            printf("countUpdated > 0\n");
                            printf("Filename boş geldi. Bütün dosyalar kontrol edildi. Yeni bir degisiklik var.\n");
                            printf("Degisiklik olan dosya: %s, new add: %d, lastModificationTime: %ld\n",updated->filename,updated->newAddition,updated->lastModificationTime);
                            if(updated->isDirectory){
                                if(updated->lastModificationTime == -1){
                                    strcpy(server_message,encoder('6',updated->filename));
                                }
                                else{
                                    strcpy(server_message,encoder('5',updated->filename));
                                }
                            }
                            else{
                                if(updated->lastModificationTime == -1){
                                    strcpy(server_message,encoder('4',updated->filename));
                                }
                                else{
                                    printf("Eski dosya\n");
                                    strcpy(server_message,encoder('1',updated->filename));
                                }
                            }
                            updated = updated->next;
                        }
                        else{
                            printf("Filename boş geldi. Bütün dosyalar kontrol edildi. Yeni bir degisiklik yok.\n");
                            strcpy(server_message,encoder('0',""));
                        }
                    }
                    //Sadece adı verilen dosyayı kontrol et
                    else{
                        if(isFileAvailableInDirectory(directoryPath,parsedCommand[1])){
                            //Eğer klasörde varsa kontrol et demeye devam et.
                            printf("Filename dolu geldi. Klasörde var. Kontrol et yollandı (Filenamesiz)\n");
                            strcpy(server_message,encoder('0',""));
                        }
/*                         else if(!isDirectoryAvailableInDirectory(directoryPath,parsedCommand[1])){
                            char* combinedPath = (char*)malloc(MAX_FILENAME * sizeof(char));
                            snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[1]);
                            printf("DIRECTORY OPENED COMMAND FROM SERVER IS SENT\n");
                            if (mkdir(combinedPath, 0777) == -1) {
                                printf("Directory exists\n");
                            }
                            strcpy(server_message,encoder('0',""));
                            free(combinedPath);
                        }
 */                        else{
                            //Eğer klasörde yoksa, yok mesajı gönder
                            printf("Filename dolu geldi. Klasörde yok. File yok komutu yollandı\n");
                            strcpy(server_message,encoder('9',parsedCommand[1]));
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
                    printf("Encoder: %s\n",encoder('0',firstStartCurrentFiles->filename));
                    strcpy(server_message,encoder('0',firstStartCurrentFiles->filename)); 
                    firstStartCurrentFiles = firstStartCurrentFiles->next;      
                }
                //Sadece adı verilen dosyayı kontrol et
                else{
                    if(isFileAvailableInDirectory(directoryPath,parsedCommand[1])){
                        //Eğer klasörde varsa kontrol et demeye devam et.
                        printf("Filename dolu geldi. Klasörde var. Senkronizasyona devam\n");
                        printf("Gönderilecek dosya: %s\n",firstStartCurrentFiles->filename);
                        printf("Encoder: %s\n",encoder('0',firstStartCurrentFiles->filename));
                        strcpy(server_message,encoder('0',firstStartCurrentFiles->filename));
                        firstStartCurrentFiles = firstStartCurrentFiles->next; 
                    }
/*                     else if(!isDirectoryAvailableInDirectory(directoryPath,parsedCommand[1])){
                        char* combinedPath = (char*)malloc(MAX_FILENAME * sizeof(char));
                        snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[1]);
                        printf("DIRECTORY OPENED COMMAND FROM SERVER IS SENT\n");
                        if (mkdir(combinedPath, 0777) == -1) {
                            printf("Directory exists\n");
                        }
                        strcpy(server_message,encoder('0',""));
                        free(combinedPath);
                    }
 */                    else{ //Directory de buraya düşecek eğer yoksa ....
                        //Eğer klasörde yoksa, yok mesajı gönder
                        printf("Filename dolu geldi. Klasörde yok.\n");
                        strcpy(server_message,encoder('9',parsedCommand[1]));
                    }
                }
            }
        }
        /* If the message is OPEN FILE */
        else if(strcmp(parsedCommand[0],"1") == 0){
            //YAPMAN GEREKEN GELEN FİLE ADINI, DİRECTORY PATH İLE BİRLEŞTİRMEK!
            char* combinedPath = (char*)malloc(MAX_FILENAME * sizeof(char));
            snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[1]);
            printf("FILE OPENED COMMAND FROM SERVER IS SENT\n");
            fd = open(combinedPath,O_WRONLY | O_CREAT | O_TRUNC, 0666);
            strcpy(server_message,encoder('8',parsedCommand[1]));
            strcpy(filePath, parsedCommand[1]);
            free(combinedPath);
        }
        /* If the message is WRITE INTO FILE */
       else if(strcmp(parsedCommand[0],"2") == 0){
            ssize_t bytesWritten = write(fd, parsedCommand[1], strlen(parsedCommand[1]));
            if (bytesWritten == -1) {
                printf("ERRRORR\n");
                perror("Error writing to the file");
                close(fd);
                //return 1;
            }
            printf("WRITTEN INTO THE FILE FROM SERVER IS SENT\n");
            strcpy(server_message,encoder('7',parsedCommand[1]));
        }
        /* If the message is CLOSE FILE */
        else if(strcmp(parsedCommand[0],"3") == 0){
            close(fd);
            currentFiles = getCurrentFilesFromDirectory(directoryPath);
            printf("File is closed in the server side\n");
            printf("CHECK COMMAND FROM SERVER IS SENT\n");
            strcpy(server_message,encoder('0',""));
        }
        /* If the message is "REMOVE FILE" */
        else if(strcmp(parsedCommand[0],"4") == 0){
            char* combinedPath = (char*)malloc(MAX_FILENAME * sizeof(char));
            snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[1]);
            remove(combinedPath);
            //currentFiles = getCurrentFilesFromDirectory(directoryPath);
            printf("FILE IS REMOVED\n");
            //memset(server_message, 0, sizeof(server_message));
            //sprintf(server_message, "%d", fd);
            strcpy(server_message,encoder('0',""));
            free(combinedPath);
        }
        else if(strcmp(parsedCommand[0],"5") == 0){
            char* combinedPath = (char*)malloc(MAX_FILENAME * sizeof(char));
            snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[1]);
            printf("DIRECTORY OPENED COMMAND FROM SERVER IS SENT\n");
            if (mkdir(combinedPath, 0777) == -1) {
                printf("Directory exists\n");
            }
            strcpy(server_message,encoder('0',""));
            free(combinedPath);
        }
        else if(strcmp(parsedCommand[0],"6") == 0){
            char* combinedPath = (char*)malloc(MAX_FILENAME * sizeof(char));
            snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[1]);
            printf("DIRECTORY REMOVE COMMAND FROM SERVER IS SENT\n");
            if (rmdir(combinedPath) == -1) {
                printf("Directory could not be removed.\n");
            }
            strcpy(server_message,encoder('0',""));
            free(combinedPath);
        }
        //Eğer burada dosyadan okunan bytesWritten == 0 veyaaa EOF ulaşmışsa, dosya kapat komutu gönder
        //Ayrıca read yapacaksın burada, okuduğunu göndereceksin.
        else if(strcmp(parsedCommand[0], "7") == 0 || strcmp(parsedCommand[0], "8") == 0){
            if(strcmp(parsedCommand[0], "8") == 0){
                char* combinedPath = (char*)malloc(MAX_FILENAME * sizeof(char));
                snprintf(combinedPath, MAX_FILENAME, "%s/%s", directoryPath, parsedCommand[1]);
                fd = open(combinedPath,O_RDONLY, 0666);
                free(combinedPath);
            }
            char * buffer = (char*)malloc(MAX_MESSAGE);
            int bytesRead = read(fd, buffer, MAX_MESSAGE);
            printf("Okunan buffer: %s\n",buffer); //Bunu bastırmadığın sürece buffer içinde çöp şeyler var
            printf("Okunan bytes: %d\n",bytesRead);
            if(bytesRead == -1){
                //Hata var
            }
            else if(bytesRead == 0){
                //Dosya okundu tamamen, dosya kapat komutu gönder
                close(fd);
                strcpy(server_message,encoder('3',parsedCommand[1]));
            }
            else{
                //Dosyaya yazmaya devam
                printf("WRITE COMMAND FROM CLIENT IS SENT\n");
                printf("Gönderilen buffer: %s\n",buffer);
                strcpy(server_message, encoder('2',buffer));
            }

            free(buffer);
        }
        else if(strcmp(parsedCommand[0],"9") == 0){
            //remove(client_message);
            printf("%s is not in client. Send open file command\n",parsedCommand[1]);
            //memset(server_message, 0, sizeof(server_message));
            //sprintf(server_message, "%d", fd);
            strcpy(server_message,encoder('1',parsedCommand[1]));
        }
        /* else{
            printf("Unknown command is received %s\n",client_message);
            strcpy(server_message,"0");
        } */
        sleep(2); //SİL
        int sent;
        if ((sent = send(client_socket, server_message, BUFFER_SIZE, 0)) <= 0) {
            perror("Error sending message to client");
        }
        //printf("The message is %s\n",printEncodedMessage(server_message));
        printf("Sent %d bytes from server to client.\n",sent);
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

    return 0;
}
