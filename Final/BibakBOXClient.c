#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUFFER_SIZE 1024

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

    strcpy(client_message, "kontrol"); // Set initial client_message to "kontrol"
    if (send(client_socket, client_message, strlen(client_message), 0) <= 0) {
            perror("Error sending message to server");
            exit(EXIT_FAILURE);
    }
    printf("Client socket = %d\n",client_socket);
    while(1){
        memset(client_message, 0, sizeof(client_message));
        if (recv(client_socket, server_message, BUFFER_SIZE, 0) <= 0) {
            perror("Error receiving message from server");
            exit(EXIT_FAILURE);
        }
        printf("%s\n", server_message);

        //Add an array and get input from terminal, put into that array. After that print that array.
        printf("Enter a message: ");
        fgets(client_message, BUFFER_SIZE, stdin);
        client_message[strcspn(client_message, "\n")] = '\0'; // Remove trailing newline character
        printf("Gönderilen %s\n",client_message);

        if (send(client_socket, client_message, strlen(client_message), 0) <= 0) {
            perror("Error sending message to server");
            exit(EXIT_FAILURE);
        }
        

    }

    // Close the socket
    close(client_socket);
}

int main() {
    send_message();
    return 0;
}
