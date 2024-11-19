#include <stdio.h>          
#include <stdlib.h>         
#include <string.h>         
#include <unistd.h>         
#include <arpa/inet.h>      
#include <netinet/in.h>     
#include <sys/socket.h>     

#define PORT 21690

void print_usage() {
    printf("Usage:\n");
    printf("1. Authentication: %s <username> <password>\n", "./client");
    printf("2. Lookup: %s lookup <username>\n", "./client");
    printf("Note: Use 'guest guest' for guest access\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        print_usage();
        return -1;
    }

    int server_fd;
    struct sockaddr_in server_address;
    char buffer[1024];
    char message[1024];

    // Create TCP socket
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

    printf("Connected to main server\n");

    // Handle different command types
    if (strcmp(argv[1], "lookup") == 0) {
        if (argc != 3) {
            printf("Error: Lookup command requires a username\n");
            print_usage();
            close(server_fd);
            return -1;
        }
        
        // Format lookup message
        snprintf(message, sizeof(message), "LOOKUP %s", argv[2]);
    } else {
        // Regular authentication
        if (argc != 3) {
            print_usage();
            close(server_fd);
            return -1;
        }
        snprintf(message, sizeof(message), "AUTH %s %s", argv[1], argv[2]);
    }

    // Send message to server
    ssize_t sent_bytes = send(server_fd, message, strlen(message), 0);
    if (sent_bytes < 0) {
        perror("Error sending message");
        close(server_fd);
        return -1;
    }

    // Receive response
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