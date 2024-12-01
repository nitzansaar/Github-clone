#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#define SERVER_D_PORT 23693
#define SERVER_R_PORT 22693
#define BUFFER_SIZE 1024

// Function to get files from Server R for a specific user
char* get_files_from_server_r(int udp_socket, struct sockaddr_in server_r_addr, const char* username) {
    static char response[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];
    
    // Send lookup request to Server R
    snprintf(buffer, BUFFER_SIZE, "LOOKUP %s SERVER_D", username);
    
    if (sendto(udp_socket, buffer, strlen(buffer), 0, 
               (struct sockaddr*)&server_r_addr, sizeof(server_r_addr)) < 0) {
        return NULL;
    }
    
    // Receive response from Server R
    socklen_t server_len = sizeof(server_r_addr);
    int response_len = recvfrom(udp_socket, response, BUFFER_SIZE - 1, 0,
                               (struct sockaddr*)&server_r_addr, &server_len);
    if (response_len < 0) {
        return NULL;
    }
    
    response[response_len] = '\0';
    return response;
}

// Function to handle deployment
char* handle_deploy(int udp_socket, struct sockaddr_in server_r_addr, const char* username) {
    static char response[BUFFER_SIZE];
    
    // First, get the list of files from Server R
    char* files = get_files_from_server_r(udp_socket, server_r_addr, username);
    if (files == NULL || strstr(files, "No files found") != NULL) {
        snprintf(response, BUFFER_SIZE, "Error: No files found for user %s in repository", username);
        return response;
    }
    
    // Create/Open deployed.txt
    FILE* deployed_file = fopen("deployed.txt", "a+");
    if (deployed_file == NULL) {
        snprintf(response, BUFFER_SIZE, "Error: Could not open deployment record");
        return response;
    }
    
    // Write deployment record
    time_t now = time(NULL);
    char timestamp[26];
    ctime_r(&now, timestamp);
    timestamp[24] = '\0';  // Remove newline
    
    fprintf(deployed_file, "=== Deployment Record ===\n");
    fprintf(deployed_file, "Time: %s\n", timestamp);
    fprintf(deployed_file, "User: %s\n", username);
    fprintf(deployed_file, "Deployed Files:\n%s", files);
    fprintf(deployed_file, "=====================\n\n");
    fclose(deployed_file);
    
    // Send success response
    snprintf(response, BUFFER_SIZE, 
             "Deployment successful!\n"
             "Time: %s\n"
             "Files deployed:\n%s", 
             timestamp, files);
    return response;
}

int main() {
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_d_addr, server_r_addr;
    
    // Setup Server D address
    memset(&server_d_addr, 0, sizeof(server_d_addr));
    server_d_addr.sin_family = AF_INET;
    server_d_addr.sin_addr.s_addr = INADDR_ANY;
    server_d_addr.sin_port = htons(SERVER_D_PORT);
    
    // Setup Server R address
    memset(&server_r_addr, 0, sizeof(server_r_addr));
    server_r_addr.sin_family = AF_INET;
    server_r_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_r_addr.sin_port = htons(SERVER_R_PORT);

    if (bind(udp_socket, (struct sockaddr*)&server_d_addr, sizeof(server_d_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Add boot-up message
    printf("Server D is up and running using UDP on port %d\n", SERVER_D_PORT);

    while (1) {
        char buffer[BUFFER_SIZE];
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        // Receive deployment request
        int bytes_received = recvfrom(udp_socket, buffer, BUFFER_SIZE - 1, 0,
                                    (struct sockaddr*)&client_addr, &client_len);
        if (bytes_received < 0) {
            continue;
        }
        
        buffer[bytes_received] = '\0';
        
        // Parse the deploy command
        char command[20], username[50];
        sscanf(buffer, "%s %s", command, username);
        
        if (strcmp(command, "DEPLOY") == 0) {
            // Add deployment request received message
            printf("Server D has received a deploy request from the main server.\n");
            
            char* deploy_response = handle_deploy(udp_socket, server_r_addr, username);
            
            // Add deployment completion message
            printf("Server D has deployed the user %s's repository.\n", username);
            
            // Send response back
            sendto(udp_socket, deploy_response, strlen(deploy_response), 0,
                   (struct sockaddr*)&client_addr, client_len);
        }
    }

    close(udp_socket);
    return 0;
}
