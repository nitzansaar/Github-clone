#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ctype.h>

#define PORT 21694
#define BUFFER_SIZE 1024

char* get_user_files(const char* username) {
    FILE* file = fopen("filenames.txt", "r");
    if (!file) {
        perror("Cannot open filenames.txt");
        return strdup("Error: Cannot access repository");
    }

    static char result[BUFFER_SIZE];
    char line[BUFFER_SIZE];
    int file_count = 0;
    
    // Initialize result
    snprintf(result, BUFFER_SIZE, "Files for user %s:\n", username);
    
    // Skip the header line
    fgets(line, sizeof(line), file);
    
    // Read each line and collect all files for the user
    while (fgets(line, sizeof(line), file)) {
        char current_username[100];
        char filename[100];
        
        // Remove newline if present
        line[strcspn(line, "\n")] = 0;
        
        // Parse the line
        sscanf(line, "%s %s", current_username, filename);
        
        // If this line is for our user
        if (strcmp(current_username, username) == 0) {
            file_count++;
            char temp[256];
            snprintf(temp, sizeof(temp), "%d. %s\n", file_count, filename);
            strcat(result, temp);
        }
    }
    
    if (file_count == 0) {
        strcat(result, "No files found.\n");
    }
    
    fclose(file);
    return result;
}

int main() {
    int server_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE];

    server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server R is up and running using UDP on port %d\n", PORT);

    while (1) {
        int len = recvfrom(server_fd, buffer, BUFFER_SIZE, 0, 
                          (struct sockaddr *)&address, (socklen_t*)&addrlen);
        buffer[len] = '\0';

        char request_type[20], username[50], requesting_user[50];
        sscanf(buffer, "%s %s %s", request_type, username, requesting_user);
        
        printf("Server R received lookup request for user: %s from user: %s\n", 
               username, requesting_user);

        char* files_list = get_user_files(username);
        printf("DEBUG - Sending response:\n%s\n", files_list);
        
        sendto(server_fd, files_list, strlen(files_list), 0,
               (struct sockaddr *)&address, addrlen);
        
        printf("Server R sent file list back to Main Server\n");
    }

    close(server_fd);
    return 0;
}