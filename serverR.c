#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ctype.h>

#define PORT 22693
#define BUFFER_SIZE 1024

/**
 * Retrieves a formatted list of files associated with a given username
 */
char* get_user_files(const char* username) {
    FILE* file = fopen("filenames.txt", "r");
    if (!file) {
        perror("Cannot open filenames.txt");
        return strdup("Error: Cannot access repository");
    }

    static char result[BUFFER_SIZE];
    char line[BUFFER_SIZE];
    int file_count = 0;
    
    memset(result, 0, BUFFER_SIZE);
    fgets(line, sizeof(line), file);  // Skip header
    
    while (fgets(line, sizeof(line), file)) {
        char current_username[100], filename[100];
        line[strcspn(line, "\n")] = 0;  // Remove newline
        sscanf(line, "%s %s", current_username, filename);
        
        if (strcmp(current_username, username) == 0) {
            file_count++;
            char temp[256];
            snprintf(temp, sizeof(temp), "%d. %s\n", file_count, filename);
            strcat(result, temp);
        }
    }
    
    if (file_count == 0) {
        strcpy(result, "No files found.\n");
    }
    
    fclose(file);
    return result;
}

/**
 * Checks if a file exists for a specific user
 */
int file_exists(const char* username, const char* filename) {
    FILE* file = fopen("filenames.txt", "r");
    if (!file) {
        perror("Cannot open filenames.txt");
        return 0;
    }

    char line[BUFFER_SIZE];
    char current_username[100], current_filename[100];
    
    // Skip header line
    fgets(line, sizeof(line), file);
    
    // Read each line and check for username/filename match
    while (fgets(line, sizeof(line), file)) {
        sscanf(line, "%s %s", current_username, current_filename);
        if (strcmp(current_username, username) == 0 && 
            strcmp(current_filename, filename) == 0) {
            fclose(file);
            return 1;  // File exists for this user
        }
    }
    
    fclose(file);
    return 0;  // File not found
}

/**
 * Adds a new file entry to the repository
 * Note: Does not check for duplicates - use file_exists() first
 */
void add_file_entry(const char* username, const char* filename) {
    FILE* file = fopen("filenames.txt", "a");
    if (!file) {
        perror("Cannot open filenames.txt for writing");
        return;
    }
    
    // Add new entry
    fprintf(file, "%s %s\n", username, filename);
    fclose(file);
}

/**
 * Removes a file entry from the repository
 */
int remove_file_entry(const char* username, const char* filename) {
    FILE* file = fopen("filenames.txt", "r");
    FILE* temp = fopen("temp.txt", "w");
    if (!file || !temp) {
        if (file) fclose(file);
        if (temp) fclose(temp);
        return 0;
    }
    
    char line[BUFFER_SIZE];
    int found = 0;
    
    // Copy header
    fgets(line, sizeof(line), file);
    fprintf(temp, "%s", line);
    
    // Copy all lines except the one to be removed
    while (fgets(line, sizeof(line), file)) {
        char current_username[100], current_filename[100];
        char line_copy[BUFFER_SIZE];
        strcpy(line_copy, line);
        sscanf(line, "%s %s", current_username, current_filename);
        
        if (strcmp(current_username, username) == 0 && 
            strcmp(current_filename, filename) == 0) {
            found = 1;
            continue;
        }
        fprintf(temp, "%s", line_copy);
    }
    
    fclose(file);
    fclose(temp);
    
    // Replace original file with temp file
    remove("filenames.txt");
    rename("temp.txt", "filenames.txt");
    
    return found;
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
        
        if (strcmp(request_type, "LOOKUP") == 0 || strcmp(request_type, "DEPLOY") == 0) {
            printf("Server R has received a %s request from the main server.\n", request_type);
            char* files_list = get_user_files(username);
            
            sendto(server_fd, files_list, strlen(files_list), 0,
                   (struct sockaddr *)&address, addrlen);
            
            printf("Server R has finished sending the response to the main server.\n");
        } else if (strcmp(request_type, "PUSH") == 0) {
            char filename[50];
            sscanf(buffer, "PUSH %s %s", username, filename);
            printf("Server R has received a push request from the main server.\n");

            // Check if file exists for this user
            if (file_exists(username, filename)) {
                printf("%s exists in %s's repository; requesting overwrite confirmation.\n", filename, username);
                sprintf(buffer, "OVERWRITE_CONFIRM");
                sendto(server_fd, buffer, strlen(buffer), 0,
                       (struct sockaddr *)&address, addrlen);
                
                // Wait for overwrite response
                len = recvfrom(server_fd, buffer, BUFFER_SIZE, 0, 
                              (struct sockaddr *)&address, (socklen_t*)&addrlen);
                buffer[len] = '\0';
                
                if (buffer[0] == 'Y' || buffer[0] == 'y') {
                    // Remove existing entry before adding new one
                    remove_file_entry(username, filename);
                    add_file_entry(username, filename);
                    printf("User requested overwrite; overwrite successful.\n");
                    sprintf(buffer, "successful");
                } else {
                    printf("Overwrite denied\n");
                    sprintf(buffer, "unsuccessful");
                }
            } else {
                // Add new file entry to filenames.txt
                add_file_entry(username, filename);
                printf("%s uploaded successfully.\n", filename);
                sprintf(buffer, "successful");
            }
            
            // Send final response
            sendto(server_fd, buffer, strlen(buffer), 0,
                   (struct sockaddr *)&address, addrlen);
        } else if (strcmp(request_type, "REMOVE") == 0) {
            char filename[50];
            sscanf(buffer, "REMOVE %s %s", username, filename);
            printf("Server R has received a remove request from the main server.\n");

            if (remove_file_entry(username, filename)) {
                sprintf(buffer, "File removed successfully");
            } else {
                sprintf(buffer, "File not found");
            }
            
            sendto(server_fd, buffer, strlen(buffer), 0,
                   (struct sockaddr *)&address, addrlen);
        }
    }

    close(server_fd);
    return 0;
}