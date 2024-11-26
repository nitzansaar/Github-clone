#include <stdio.h>          
#include <stdlib.h>         
#include <string.h>         
#include <unistd.h>         
#include <arpa/inet.h>      
#include <netinet/in.h>     
#include <sys/socket.h>     
#include <ctype.h> 
#include <stdbool.h>
#define PORT 21690
//add another port for guests so they can both connect at the same time
#define GUEST_PORT 21691

// Add these color definitions at the top of the file
#define PURPLE "\033[35m"
#define RESET "\033[0m"

/*
After the client gets authenticated, it doesnt exit the program. You are basically 'logged in' to the server.
You can then use the lookup and push commands to interact with the server.
Currently, we exit the program after authentication.
So we need to change that so that the client can keep running and interacting with the server.
*/

// Store the authenticated username globally
char authenticated_user[50] = {0};

// Add this with the other function prototypes near the top of the file
void log_operation(const char* username, const char* operation, const char* details);

void handle_lookup_command(int server_fd, const char* command, const char* username, int parsed);
void handle_push_command(int server_fd, const char* command, const char* filename, int parsed);
void handle_deploy_command(int server_fd, const char* command, const char* unused, int parsed);
int setup_connection(struct sockaddr_in* server_address, bool is_guest);
int authenticate_user(int server_fd, const char* username, const char* password);
void handle_remove_command(int server_fd, const char* command, const char* filename, int parsed);
void handle_log_command(int server_fd);

void print_usage() {
    printf("Usage:\n");
    printf("1. Authentication: %s <username> <password>\n", "./client");
    printf("2. Lookup: %s lookup <username>\n", "./client");
    printf("Note: Use 'guest guest' for guest access\n");
}

void handle_lookup_command(int server_fd, const char* command, const char* username, int parsed) {
    char buffer[1024];
    char lookup_msg[1024];
    
    // Check if username is specified
    if (parsed != 2) {
        printf("Error: Username is required. Please specify a username to lookup.\n");
        printf("----Start a new request----\n");
        return;
    }
    
    // Construct lookup message
    snprintf(lookup_msg, sizeof(lookup_msg), "LOOKUP %s", username);
    
    // Send lookup request
    if (send(server_fd, lookup_msg, strlen(lookup_msg), 0) < 0) {
        perror("Error sending lookup request");
        return;
    }
    
    // Print sent message for guest
    printf("Guest sent a lookup request to the main server.\n");

    // Receive response
    int len = recv(server_fd, buffer, sizeof(buffer) - 1, 0);
    if (len < 0) {
        perror("Error receiving lookup response");
        return;
    }
    buffer[len] = '\0';
    
    // Get the client's port number
    struct sockaddr_in local_addr;
    socklen_t addr_len = sizeof(local_addr);
    getsockname(server_fd, (struct sockaddr*)&local_addr, &addr_len);
    int client_port = ntohs(local_addr.sin_port);
    
    // Print standard response header
    printf("The client received the response from the main server using TCP over port %d.\n", 
           client_port);

    // Handle different response cases
    if (strstr(buffer, "does not exist") != NULL) {
        // Username doesn't exist case
        printf("%s does not exist. Please try again.\n", username);
    } else if (strstr(buffer, "Empty repository") != NULL || 
               strstr(buffer, "No files found") != NULL) {
        // Empty repository case
        printf("Empty repository.\n");
    } else {
        // Normal case with files
        printf("%s\n", buffer);
    }
    
    // Always print the delimiter
    printf("----Start a new request----\n");
}

int setup_connection(struct sockaddr_in* server_address, bool is_guest) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("error creating client socket");
        return -1;
    }

    server_address->sin_family = AF_INET;
    if (is_guest) {
        server_address->sin_port = htons(GUEST_PORT);
    } else {
        server_address->sin_port = htons(PORT);
    }
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
    printf("test\n");

    // Receive response
    int len = recv(server_fd, buffer, sizeof(buffer) - 1, 0);
    if (len < 0) {
        perror("Error receiving response");
        return -1;
    }
    buffer[len] = '\0';
    printf("\n%s\n", buffer);

    if (strcmp(username, "guest") == 0 && strcmp(password, "guest") == 0) {
        printf("You have been granted guest access.\n");
        return 1;
    } else if (strstr(buffer, "Authentication successful") != NULL) {
        printf("You have been granted member access\n");
        return 1;
    } else {
        printf("The credentials are incorrect. Please try again.\n");
        return 0;
    }
}

void handle_push_command(int server_fd, const char* command, const char* filename, int parsed) {
    char buffer[1024];
    char push_msg[1024];
    
    // Check if filename is specified
    if (parsed != 2) {
        printf("Error: Filename is required. Please specify a filename to push.\n");
        return;
    }
    
    // Check if file exists and is readable
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: Invalid file: %s\n", filename);
        printf("----Start a new request----\n");
        return;
    }
    fclose(file);
    
    // Construct push message
    snprintf(push_msg, sizeof(push_msg), "PUSH %s", filename);
    
    // Send push request
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
    
    // Handle overwrite confirmation if needed
    if (strstr(buffer, "exists") != NULL) {
        printf("%s exists in %s's repository, do you want to overwrite (Y/N)? ", 
               filename, authenticated_user);
        
        char answer[10];
        fgets(answer, sizeof(answer), stdin);
        answer[strcspn(answer, "\n")] = '\0';
        
        // Send overwrite response
        send(server_fd, answer, strlen(answer), 0);
        
        // Get final response
        len = recv(server_fd, buffer, sizeof(buffer) - 1, 0);
        buffer[len] = '\0';
    }
    
    // Print final result
    if (strstr(buffer, "successfully") != NULL) {
        printf("%s pushed successfully\n", filename);
    } else {
        printf("%s was not pushed successfully.\n", filename);
    }
}

void handle_deploy_command(int server_fd, const char* command, const char* unused, int parsed) {
    char buffer[1024];
    
    // Send deploy request
    if (send(server_fd, "DEPLOY", strlen("DEPLOY"), 0) < 0) {
        perror("Error sending deploy request");
        return;
    }
    
    // Print sent message
    printf("%s sent a lookup request to the main server.\n", authenticated_user);

    // Receive response
    int len = recv(server_fd, buffer, sizeof(buffer) - 1, 0);
    if (len < 0) {
        perror("Error receiving deploy response");
        return;
    }
    buffer[len] = '\0';
    
    // Get the client's port number
    struct sockaddr_in local_addr;
    socklen_t addr_len = sizeof(local_addr);
    getsockname(server_fd, (struct sockaddr*)&local_addr, &addr_len);
    int client_port = ntohs(local_addr.sin_port);
    
    // Print response header
    printf("The client received the response from the main server using TCP over port %d.\n", 
           client_port);
           
    if (strstr(buffer, "Error") == NULL && strlen(buffer) > 0) {
        printf("The following files in his/her repository have been deployed.\n");
        printf("%s\n", buffer);
    } else {
        printf("%s\n", buffer);
    }
    
    printf("----Start a new request----\n");
}

void handle_remove_command(int server_fd, const char* command, const char* filename, int parsed) {
    char buffer[1024];
    char remove_msg[1024];
    
    // Check if command format is correct
    if (parsed != 2) {
        printf("Invalid remove command format. Use: remove <filename>\n");
        return;
    }
    
    // Construct remove message
    snprintf(remove_msg, sizeof(remove_msg), "REMOVE %s", filename);
    
    // Send remove request
    if (send(server_fd, remove_msg, strlen(remove_msg), 0) < 0) {
        perror("Error sending remove request");
        return;
    }
    
    // Print sent message
    printf("%s sent a remove request to the main server.\n", authenticated_user);

    // Receive response
    int len = recv(server_fd, buffer, sizeof(buffer) - 1, 0);
    if (len < 0) {
        perror("Error receiving remove response");
        return;
    }
    buffer[len] = '\0';
    
    // Print result
    if (strstr(buffer, "successfully") != NULL) {
        printf("The remove request was successful.\n");
    } else {
        printf("The remove request was not successful.\n");
    }
}

void handle_log_command(int server_fd) {
    char buffer[1024];
    
    // Send log request
    if (send(server_fd, "LOG", strlen("LOG"), 0) < 0) {
        perror("Error sending log request");
        return;
    }
    
    // Print sent message with "log" in purple
    printf("%s sent a %slog%s request to the main server.\n", 
           authenticated_user, PURPLE, RESET);

    // Receive response
    int len = recv(server_fd, buffer, sizeof(buffer) - 1, 0);
    if (len < 0) {
        perror("Error receiving log response");
        return;
    }
    buffer[len] = '\0';
    
    // Get the client's port number
    struct sockaddr_in local_addr;
    socklen_t addr_len = sizeof(local_addr);
    getsockname(server_fd, (struct sockaddr*)&local_addr, &addr_len);
    int client_port = ntohs(local_addr.sin_port);
    
    // Print response header
    printf("The client received the response from the main server using TCP over port %d.\n", 
           client_port);
    
    // Print the numbered list of operations
    printf("%s\n", buffer);
    
    // Print delimiter
    printf("----Start a new request----\n");
}

void log_operation(const char* username, const char* operation, const char* details) {
    char log_msg[1024];
    snprintf(log_msg, sizeof(log_msg), "LOG_ENTRY %s %s %s", username, operation, details);
    
    FILE* log_file = fopen("operations.log", "a");
    if (log_file) {
        fprintf(log_file, "%s\n", log_msg);
        fclose(log_file);
    }
}

// Add this near the top with other global variables
bool is_guest = false;

// Update the main function to set the guest flag
int main(int argc, char* argv[]) {
    printf("%sThe client is up and running.%s\n", PURPLE, RESET);
    
    if (argc < 2 || argc > 3) {
        print_usage();
        return -1;
    }

    // Set guest flag if credentials are "guest guest"
    is_guest = (argc == 3 && strcmp(argv[1], "guest") == 0 && strcmp(argv[2], "guest") == 0);

    struct sockaddr_in server_address;
    char buffer[1024];
    char message[1024];

    int server_fd = setup_connection(&server_address, is_guest);
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
        if (is_guest) {
            printf("Please enter the command: <lookup <username>>\n");
        } else {
            printf("Please enter the command: <lookup <username>> , <push <filename> > , "
                   "<remove <filename> > , <deploy> , <log>.\n");
        }
        
        fgets(message, sizeof(message), stdin);
        message[strcspn(message, "\n")] = '\0';
        
        char command[20], username[50];
        int parsed = sscanf(message, "%s %s", command, username);
        
        // For guests, only allow lookup command
        if (is_guest && strcmp(command, "lookup") != 0) {
            printf("Guests can only use the lookup command\n");
            continue;
        }
        
        // Handle commands as before
        if (parsed >= 1 && strcmp(command, "lookup") == 0) {
            handle_lookup_command(server_fd, command, username, parsed);
        } else if (!is_guest) {  // Only allow these commands for non-guests
            if (strcmp(command, "push") == 0) {
                handle_push_command(server_fd, command, username, parsed);
            } else if (strcmp(command, "deploy") == 0) {
                handle_deploy_command(server_fd, command, username, parsed);
            } else if (strcmp(command, "remove") == 0) {
                handle_remove_command(server_fd, command, username, parsed);
            } else if (strcmp(command, "log") == 0) {
                handle_log_command(server_fd);
            } else {
                if (is_guest) {
                    printf("Guests can only use the lookup command\n");
                } else {
                    printf("Invalid command. Available commands:\n");
                    printf("1. lookup <username>\n");
                    printf("2. push <filename>\n");
                    printf("3. deploy <filename>\n");
                    printf("4. remove <filename>\n");
                    printf("5. log\n");
                }
            }
        }
    }
    
    close(server_fd);
    return 0;
}