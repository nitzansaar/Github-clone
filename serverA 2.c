#include <stdio.h>          
#include <stdlib.h>         
#include <string.h>         
#include <unistd.h>         
#include <arpa/inet.h>      
#include <netinet/in.h>     
#include <sys/socket.h>     
#include <ctype.h>
#include <errno.h>  

#define PORT 21693 // last 3 digits of my student id are '693'
void decrypt(char* encrypted, char* decrypted);
int authenticate(char *username, char *password);


int main() {
    // authenticate("sup", "fam");
    int server_fd;  
    struct sockaddr_in address;
    int addrlen = sizeof(address);    
    char buffer[1024];
    
    // 1. Create UDP server socket
    server_fd = socket(AF_INET, SOCK_DGRAM, 0); // Use UDP
    if (server_fd == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 2. Configure the server address structure
    address.sin_family = AF_INET; 
    address.sin_addr.s_addr = INADDR_ANY; 
    address.sin_port = htons(PORT);

    // 3. Bind the socket to the specified IP address and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("Server A is up and running using UDP on port %d...\n", PORT);

    // 4. Handle incoming authentication requests in a loop
    while (1) {
        // Receive authentication request from the main server
        int len = recvfrom(server_fd, buffer, 1024, 0, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        buffer[len] = '\0'; // Null-terminate the received data

        // Parse and authenticate here (split buffer into username and password)
        char username[50], password[50];
        sscanf(buffer, "%s %s", username, password); // Adjust based on format
        printf("ServerA received username %s and password %s\n", username, password);

        const char* response;
        if (authenticate(username, password) == 1) {
            response = "Authentication successful";
        } else {
            response = "Authentication failed";
        }
        // Send back success or error message based on authentication result
        sendto(server_fd, response, strlen(response), 0,
                                (struct sockaddr *)&address, addrlen);
        

    }

    close(server_fd);

    return 0;
}

void decrypt(char* encrypted, char* decrypted) {
    printf("decrypting %s %s", encrypted, decrypted);
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
    printf("authenticating %s %s\n", username, password);
    
    // Open both files
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
        
        printf("Checking user: %s\n", encrypted_username);
        
        // Verify username matches in both files
        if (strcmp(encrypted_username, original_username) != 0) {
            printf("Error: Username mismatch between files\n");
            continue;
        }
        
        // Check if this is the user we're looking for
        if (strcmp(username, encrypted_username) == 0) {
            found = 1;
            char decrypted_password[50];
            decrypt(encrypted_password, decrypted_password);
            
            printf("Found user %s\n", username);
            printf("Encrypted password: %s\n", encrypted_password);
            printf("Decrypted password: %s\n", decrypted_password);
            printf("Original password: %s\n", original_password);
            printf("Provided password: %s\n", password);
            
            // If decryption works and provided password matches decrypted password
            if (strcmp(decrypted_password, original_password) == 0) {
                printf("%s has been authenticated\n", username);
                fclose(encrypted_file);
                fclose(original_file);
                return 1; // success
            } else {
                printf("Password incorrect for user %s\n", username);
                fclose(encrypted_file);
                fclose(original_file);
                return 0; // fail
            }
        }
    }
    
    if (!found) {
        printf("Username %s not found\n", username);
    }
    
    fclose(encrypted_file);
    fclose(original_file);
    return 0;
}