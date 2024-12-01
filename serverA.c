#include <stdio.h>          
#include <stdlib.h>         
#include <string.h>         
#include <unistd.h>         
#include <arpa/inet.h>      
#include <netinet/in.h>     
#include <sys/socket.h>     
#include <ctype.h>
#include <errno.h>  

#define PORT 21693    // Changed from 21000
void decrypt(char* encrypted, char* decrypted);
int authenticate(char *username, char *password);
const char* check_credentials(char* username, char* password);



int main() {
    int server_fd;  
    struct sockaddr_in address;
    int addrlen = sizeof(address);    
    char buffer[1024];
    
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
    printf("Server A is up and running using UDP on port %d\n", PORT);

    while (1) {
        int len = recvfrom(server_fd, buffer, 1024, 0, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        buffer[len] = '\0';

        char username[50], password[50];
        sscanf(buffer, "%s %s", username, password);
        printf("ServerA received username %s and password ******\n", username);

        // Get authentication response with user type
        const char* response = check_credentials(username, password);
        
        sendto(server_fd, response, strlen(response), 0,
               (struct sockaddr *)&address, addrlen);
    }

    close(server_fd);
    return 0;
}

// New function to check credentials and return appropriate response
const char* check_credentials(char* username, char* password) {
    // Check for guest login
    if (strcmp(username, "guest") == 0 && strcmp(password, "guest") == 0) {
        return "GUEST_AUTH_SUCCESS";
    }
    
    // For regular members, use existing authentication
    if (authenticate(username, password) == 1) {
        return "MEMBER_AUTH_SUCCESS";
    }
    
    return "AUTH_FAILED";
}

void decrypt(char* encrypted, char* decrypted) {
    int i = 0;
    while (encrypted[i] != '\0') {
        if (isalpha(encrypted[i])) {
            // Adjust alphabetic characters
            if (isupper(encrypted[i])) {
                decrypted[i] = ((encrypted[i] - 'A' - 3 + 26) % 26) + 'A';  // cyclic wrap for uppercase
            } else {
                decrypted[i] = ((encrypted[i] - 'a' - 3 + 26) % 26) + 'a';  // cyclic wrap for lowercase
            }
        } else if (isdigit(encrypted[i])) {
            // Adjust numeric characters
            decrypted[i] = ((encrypted[i] - '0' - 3 + 10) % 10) + '0';  // cyclic wrap for digits
        } else {
            // Keep special characters unchanged`
            decrypted[i] = encrypted[i];
        }
        i++;
    }
    decrypted[i] = '\0';
}

int authenticate(char *username, char *password) {
    
    FILE *encrypted_file = fopen("members.txt", "r");
    FILE *original_file = fopen("original.txt", "r");
    
    if (!encrypted_file || !original_file) {
        perror("Failed to open one or both files");
        if (encrypted_file) fclose(encrypted_file);
        if (original_file) fclose(original_file);
        return -1;
    }
    
    char encrypted_line[1024], original_line[1024];
    char encrypted_username[50], encrypted_password[50];
    char original_username[50], original_password[50];
    int found = 0;
    
    // Read through both files simultaneously
    while (fgets(encrypted_line, sizeof(encrypted_line), encrypted_file) != NULL &&
           fgets(original_line, sizeof(original_line), original_file) != NULL) {
        
        // Remove newlines if present
        encrypted_line[strcspn(encrypted_line, "\n")] = 0;
        original_line[strcspn(original_line, "\n")] = 0;
        
        // Parse both lines
        sscanf(encrypted_line, "%s %s", encrypted_username, encrypted_password);
        sscanf(original_line, "%s %s", original_username, original_password);
        
        
        // Check if this is the user we're looking for
        if (strcmp(username, encrypted_username) == 0) {
            found = 1;
            char decrypted_password[50];
            decrypt(encrypted_password, decrypted_password);
            
            
            // Compare with provided password
            if (strcmp(password, original_password) == 0) {
                printf("Member %s has been authenticated\n", username);
                fclose(encrypted_file);
                fclose(original_file);
                return 1; // success
            } else {
                printf("The username %s or password ****** is incorrect\n", username);
                fclose(encrypted_file);
                fclose(original_file);
                return 0; // fail
            }
        }
    }
    
    if (!found) {
        printf("Debug: Username %s not found in files\n", username);
    }
    
    fclose(encrypted_file);
    fclose(original_file);
    return 0;
}