#include <stdio.h>          
#include <stdlib.h>         
#include <string.h>         
#include <unistd.h>         
#include <arpa/inet.h>      
#include <netinet/in.h>     
#include <sys/socket.h>     
#include <ctype.h>
#include <errno.h>  

#define PORT 21693

// Function prototypes with descriptive comments
// Decrypts text using a Caesar cipher with shift of 3
void decrypt(char* encrypted, char* decrypted);
// Validates user credentials against stored credentials
int authenticate(char *username, char *password);
// Determines user type and authentication status
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
        // Receive and parse credentials from client
        int len = recvfrom(server_fd, buffer, 1024, 0, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        buffer[len] = '\0';

        char username[50], password[50];
        sscanf(buffer, "%s %s", username, password);
        printf("ServerA received username %s and password ******\n", username);

        const char* response = check_credentials(username, password);
        sendto(server_fd, response, strlen(response), 0,
               (struct sockaddr *)&address, addrlen);
    }

    close(server_fd);
    return 0;
}

const char* check_credentials(char* username, char* password) {
    if (strcmp(username, "guest") == 0 && strcmp(password, "guest") == 0) {
        return "GUEST_AUTH_SUCCESS";
    }
    
    // Regular member authentication
    return (authenticate(username, password) == 1) ? "MEMBER_AUTH_SUCCESS" : "AUTH_FAILED";
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
    
    // Validate file handles
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
    
    // Read and compare credentials from both files
    while (fgets(encrypted_line, sizeof(encrypted_line), encrypted_file) != NULL &&
           fgets(original_line, sizeof(original_line), original_file) != NULL) {
        
        // Remove newlines and parse credentials
        encrypted_line[strcspn(encrypted_line, "\n")] = 0;
        original_line[strcspn(original_line, "\n")] = 0;
        sscanf(encrypted_line, "%s %s", encrypted_username, encrypted_password);
        sscanf(original_line, "%s %s", original_username, original_password);
        
        if (strcmp(username, encrypted_username) == 0) {
            found = 1;
            // Compare with provided password using original (unencrypted) credentials
            if (strcmp(password, original_password) == 0) {
                printf("Member %s has been authenticated\n", username);
                fclose(encrypted_file);
                fclose(original_file);
                return 1;
            }
            printf("The username %s or password ****** is incorrect\n", username);
            break;
        }
    }
    
    if (!found) {
        printf("Debug: Username %s not found in files\n", username);
    }
    
    fclose(encrypted_file);
    fclose(original_file);
    return 0;
}