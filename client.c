#include <stdio.h>          
#include <stdlib.h>         
#include <string.h>         
#include <unistd.h>         
#include <arpa/inet.h>      
#include <netinet/in.h>     
#include <sys/socket.h>     

#define PORT 21690

/*
After the client gets authenticated, it doesnt exit the program. You are basically 'logged in' to the server.
You can then use the lookup and push commands to interact with the server.
Currently, we exit the program after authentication.
So we need to change that so that the client can keep running and interacting with the server.
*/

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

    // first we authenticate
    if (argc == 3) {
        snprintf(message, sizeof(message), "AUTH %s %s", argv[1], argv[2]);
    } else {
        print_usage();
        close(server_fd);
        return -1;
    }

    // // Handle different command types
    // if (strcmp(argv[1], "lookup") == 0) {
    //     if (argc != 3) {
    //         printf("Error: Lookup command requires a username\n");
    //         print_usage();
    //         close(server_fd);
    //         return -1;
    //     }
        
    //     // Format lookup message
    //     snprintf(message, sizeof(message), "LOOKUP %s", argv[2]);
    //     printf("Sent message: %s\n", message);
    // }

    // Send message to server
    ssize_t sent_bytes = send(server_fd, message, strlen(message), 0);
    printf("Sent message: %s\n", message);
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

    // now we are authenticated, we can send other commands
    while (1) {
        printf("Enter command: ");
        fgets(message, sizeof(message), stdin);
        message[strcspn(message, "\n")] = '\0';
        // check for lookup command
        char command[20], username[50];
        if (sscanf(message, "%s %s", command, username) == 2 && strcmp(command, "lookup") == 0) {
            // Format lookup message
            char lookup_msg[1024];
            snprintf(lookup_msg, sizeof(lookup_msg), "LOOKUP %s", username); // Simplified lookup format
            printf("Sent message: %s\n", lookup_msg);
            printf("Looking up files for user %s...\n", username);
            
            // Send lookup request
            if (send(server_fd, lookup_msg, strlen(lookup_msg), 0) < 0) {
                perror("Error sending lookup request");
                continue;
            }

            // Receive response
            int len = recv(server_fd, buffer, sizeof(buffer) - 1, 0);
            if (len < 0) {
                perror("Error receiving lookup response");
                continue;
            }
            buffer[len] = '\0';
            
            // Print the file list
            printf("\nFile list received:\n%s\n", buffer);
        } else {
            printf("Invalid command. Use format: lookup <username>\n");
        }


    }
    
    close(server_fd);
    return 0;
}