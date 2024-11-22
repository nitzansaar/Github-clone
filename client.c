#include <stdio.h>          
#include <stdlib.h>         
#include <string.h>         
#include <unistd.h>         
#include <arpa/inet.h>      
#include <netinet/in.h>     
#include <sys/socket.h>     
#include <ctype.h>  // Add this at the top with other includes

#define PORT 21690

/*
After the client gets authenticated, it doesnt exit the program. You are basically 'logged in' to the server.
You can then use the lookup and push commands to interact with the server.
Currently, we exit the program after authentication.
So we need to change that so that the client can keep running and interacting with the server.
*/

// Store the authenticated username globally
char authenticated_user[50] = {0};

// Move these function prototypes to be together at the top
void handle_lookup_command(int server_fd, const char* command, const char* username, int parsed);
void handle_push_command(int server_fd, const char* command, const char* filename, int parsed);
void handle_deploy_command(int server_fd, const char* command, const char* unused, int parsed);
int setup_connection(struct sockaddr_in* server_address);
int authenticate_user(int server_fd, const char* username, const char* password);
void handle_remove_command(int server_fd, const char* command, const char* filename, int parsed);

void print_usage() {
    printf("Usage:\n");
    printf("1. Authentication: %s <username> <password>\n", "./client");
    printf("2. Lookup: %s lookup <username>\n", "./client");
    printf("Note: Use 'guest guest' for guest access\n");
}

void handle_lookup_command(int server_fd, const char* command, const char* username, int parsed) {
    char buffer[1024];
    char lookup_msg[1024];
    
    if (parsed == 1) {
        // No username specified, use authenticated user
        snprintf(lookup_msg, sizeof(lookup_msg), "LOOKUP %s", authenticated_user);
        printf("Looking up your files...\n");
    } else {
        // Username specified
        snprintf(lookup_msg, sizeof(lookup_msg), "LOOKUP %s", username);
        printf("Looking up files for user %s...\n", username);
    }
    
    // Send lookup request
    printf("Sent message: %s\n", lookup_msg);
    
    if (send(server_fd, lookup_msg, strlen(lookup_msg), 0) < 0) {
        perror("Error sending lookup request");
        return;
    }

    // Receive response
    int len = recv(server_fd, buffer, sizeof(buffer) - 1, 0);
    if (len < 0) {
        perror("Error receiving lookup response");
        return;
    }
    buffer[len] = '\0';
    
    // Print the file list
    printf("\nFile list received:\n%s\n", buffer);
}

int setup_connection(struct sockaddr_in* server_address) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("error creating client socket");
        return -1;
    }

    server_address->sin_family = AF_INET;
    server_address->sin_port = htons(PORT);
    server_address->sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(server_fd, (struct sockaddr*)server_address, sizeof(*server_address)) < 0) {
        perror("Connection failed");
        close(server_fd);
        return -1;
    }

    printf("Connected to main server\n");
    return server_fd;
}

int authenticate_user(int server_fd, const char* username, const char* password) {
    char buffer[1024];
    char message[1024];

    snprintf(message, sizeof(message), "AUTH %s %s", username, password);

    // Send message to server
    ssize_t sent_bytes = send(server_fd, message, strlen(message), 0);
    printf("Sent message: %s\n", message);
    if (sent_bytes < 0) {
        perror("Error sending message");
        return -1;
    }

    // Receive response
    int len = recv(server_fd, buffer, sizeof(buffer) - 1, 0);
    if (len < 0) {
        perror("Error receiving response");
        return -1;
    }
    buffer[len] = '\0';
    printf("\n%s\n", buffer);

    // Return 1 if authentication successful, 0 otherwise
    return (strstr(buffer, "Authentication successful") != NULL);
}

void handle_push_command(int server_fd, const char* command, const char* filename, int parsed) {
    char buffer[1024];
    char push_msg[1024];
    
    // Check if command format is correct (push <filename>)
    if (parsed != 2) {
        printf("Invalid push command format. Use: push <filename>\n");
        return;
    }
    
    // Construct push message with authenticated user and filename
    snprintf(push_msg, sizeof(push_msg), "PUSH %s %s", authenticated_user, filename);
    printf("Requesting push for file: %s\n", filename);
    
    // Send push request
    printf("Sent message: %s\n", push_msg);
    if (send(server_fd, push_msg, strlen(push_msg), 0) < 0) {
        perror("Error sending push request");
        return;
    }

    // Receive initial response
    int len = recv(server_fd, buffer, sizeof(buffer) - 1, 0);
    if (len < 0) {
        perror("Error receiving push response");
        return;
    }
    buffer[len] = '\0';
    
    // Check if we need to handle overwrite confirmation
    if (strstr(buffer, "File exists") != NULL) {
        printf("%s\n", buffer);
        printf("Do you want to overwrite? (Y/N): ");
        
        char response[10];
        fgets(response, sizeof(response), stdin);
        response[strcspn(response, "\n")] = '\0';
        
        // Convert response to uppercase for case-insensitive comparison
        for (int i = 0; response[i]; i++) {
            response[i] = toupper(response[i]);
        }
        
        // Send overwrite response
        snprintf(push_msg, sizeof(push_msg), "OVERWRITE %s", response);
        if (send(server_fd, push_msg, strlen(push_msg), 0) < 0) {
            perror("Error sending overwrite response");
            return;
        }
        
        // Receive final response
        len = recv(server_fd, buffer, sizeof(buffer) - 1, 0);
        if (len < 0) {
            perror("Error receiving final response");
            return;
        }
        buffer[len] = '\0';
    }
    
    // Print the server's response
    printf("\nServer response:\n%s\n", buffer);
}

void handle_deploy_command(int server_fd, const char* command, const char* unused, int parsed) {
    char buffer[1024];
    char deploy_msg[1024];
    
    // Construct deploy message with authenticated user
    snprintf(deploy_msg, sizeof(deploy_msg), "DEPLOY %s", authenticated_user);
    printf("Requesting deployment for all files of user: %s\n", authenticated_user);
    
    // Send deploy request
    printf("Sent message: %s\n", deploy_msg);
    if (send(server_fd, deploy_msg, strlen(deploy_msg), 0) < 0) {
        perror("Error sending deploy request");
        return;
    }

    // Receive response
    int len = recv(server_fd, buffer, sizeof(buffer) - 1, 0);
    if (len < 0) {
        perror("Error receiving deploy response");
        return;
    }
    buffer[len] = '\0';
    
    // Print the server's response
    printf("\nServer response:\n%s\n", buffer);
}

void handle_remove_command(int server_fd, const char* command, const char* filename, int parsed) {
    char buffer[1024];
    char remove_msg[1024];
    
    // Check if command format is correct (remove <filename>)
    if (parsed != 2) {
        printf("Invalid remove command format. Use: remove <filename>\n");
        return;
    }
    
    // Construct remove message with authenticated user and filename
    snprintf(remove_msg, sizeof(remove_msg), "REMOVE %s", filename);
    printf("Requesting removal of file: %s\n", filename);
    
    // Send remove request
    if (send(server_fd, remove_msg, strlen(remove_msg), 0) < 0) {
        perror("Error sending remove request");
        return;
    }

    // Receive response
    int len = recv(server_fd, buffer, sizeof(buffer) - 1, 0);
    if (len < 0) {
        perror("Error receiving remove response");
        return;
    }
    buffer[len] = '\0';
    
    // Print the server's response
    printf("\nServer response:\n%s\n", buffer);
}

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        print_usage();
        return -1;
    }

    struct sockaddr_in server_address;
    char buffer[1024];
    char message[1024];

    int server_fd = setup_connection(&server_address);
    if (server_fd < 0) {
        return -1;
    }

    // Authenticate user
    if (argc == 3) {
        if (authenticate_user(server_fd, argv[1], argv[2]) > 0) {
            strncpy(authenticated_user, argv[1], sizeof(authenticated_user) - 1);
        } else {
            close(server_fd);
            return -1;
        }
    } else {
        print_usage();
        close(server_fd);
        return -1;
    }

    // now we are authenticated, we can send other commands
    while (1) {
        printf("Enter command: ");
        fgets(message, sizeof(message), stdin);
        message[strcspn(message, "\n")] = '\0';
        
        char command[20], username[50];
        int parsed = sscanf(message, "%s %s", command, username);
        
        if (parsed >= 1 && strcmp(command, "lookup") == 0) {
            handle_lookup_command(server_fd, command, username, parsed);
        } else if (strcmp(command, "push") == 0) {
            // handle push command
            handle_push_command(server_fd, command, username, parsed);
        } else if (strcmp(command, "deploy") == 0) {
            handle_deploy_command(server_fd, command, username, parsed);
        } else if (strcmp(command, "remove") == 0) {
            handle_remove_command(server_fd, command, username, parsed);
        } else {
            printf("Invalid command. Available commands:\n");
            printf("1. lookup <username>\n");
            printf("2. push <filename>\n");
            printf("3. deploy <filename>\n");
            printf("4. remove <filename>\n");
        }


    }
    
    close(server_fd);
    return 0;
}