#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_CLIENTS 5
#define BUFFER_SIZE 1024

int currentThreadIndex = 0;

void* handle_client(void* arg) {
    int client_socket = *(int*)arg;
    char client_message[BUFFER_SIZE];
    char server_message[BUFFER_SIZE];
    int fd;
    printf("Client socket = %d\n",client_socket);
    // Receive message from client
    while(1){
        printf("(SERVER)Before receive file change\n");
        memset(client_message, 0, sizeof(client_message));
        if (recv(client_socket, client_message, BUFFER_SIZE, 0) <= 0) {
            perror("Error receiving message from client");
            close(client_socket);
            pthread_exit(NULL);
        }
        printf("(SERVER)After receive file change\n");
        printf("%s\n", client_message);

        if(strcmp(client_message,"kontrol") == 0){
            printf("Gardaş ben kontrol ettim senin klasörde degisiklik yok\n");
            strcpy(server_message,"Gardaş ben kontrol ettim senin klasörde degisiklik yok");
        }
        else if(strcmp(client_message,"open") == 0){
            fd = open("test.txt",O_WRONLY | O_CREAT | O_TRUNC, 0666);
            printf("Gardaş attığın dosyayı açtım bu da file descriptoru. Bana geri yolla\n");
            memset(server_message, 0, sizeof(server_message));
            sprintf(server_message, "%d", fd);
            printf("FD= %s\n",server_message);
        }
        else if(strcmp(client_message,"ARDA") == 0){
            ssize_t bytesWritten = write(fd, client_message, strlen(client_message));
            if (bytesWritten == -1) {
                printf("ERRRORR\n");
                perror("Error writing to the file");
                close(fd);
                //return 1;
            }
            printf("Gardaş bana bi dosya yollamışsın ben onu yaziyorum haberin olsun la\n");
            strcpy(server_message,"Gardaş bana bi dosya yollamışsın ben onu yaziyorum haberin olsun la");
        }
        else if(strcmp(client_message,"upload") == 0){
            close(fd);
            strcpy(server_message,"Gardaş dosyayı kapattım haberin olsun");
        }
        else{
            printf("Gardaş bu komut ne ben anlamadım la\n");
            strcpy(server_message,"Gardaş bu komut ne ben anlamadım la");
        }


        // Send message to client
        printf("(SERVER)Before send file change\n");
        if (send(client_socket, server_message, strlen(server_message), 0) <= 0) {
            perror("Error sending message to client");
        }
        printf("(SERVER)After send file change\n");
        
    }
    // Close the client socket
    close(client_socket);
    pthread_exit(NULL);
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_address, client_address;
    socklen_t client_address_length;
    pthread_t threads[MAX_CLIENTS];

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
