#include <stdio.h>          
#include <stdlib.h>         
#include <string.h>         
#include <unistd.h>         
#include <arpa/inet.h>      
#include <netinet/in.h>     
#include <sys/socket.h>     

#define PORT 21690

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s <username> <password>\n", argv[0]);
        printf("Note: Use 'guest guest' for guest access\n");
        return -1;
    }

    char* username = argv[1];
    char* password = argv[2];

    int server_fd;
    struct sockaddr_in server_address;
    char buffer[1024];
    char message[100];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("error creating client socket");
        return -1;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(server_fd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("Connection failed");
        close(server_fd);
        return -1;
    }

    snprintf(message, sizeof(message), "%s %s", username, password);

    printf("Connected to main server\n");
    printf("Sending authentication request...\n");

    ssize_t sent_bytes = send(server_fd, message, strlen(message), 0);
    if (sent_bytes < 0) {
        perror("error sending authentication message");
        close(server_fd);
        return -1;
    }

    int len = recv(server_fd, buffer, sizeof(buffer) - 1, 0);
    if (len < 0) {
        perror("Error receiving response");
        close(server_fd);
        return -1;
    }
    buffer[len] = '\0';
    printf("\n%s\n", buffer);
    
    close(server_fd);
    return 0;
}