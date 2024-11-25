#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ctype.h>
#include <time.h>

#define MAIN_TCP_PORT 21690
#define SERVER_A_PORT 21693
#define SERVER_R_PORT 21694
#define SERVER_D_PORT 21695
#define BUFFER_SIZE 1024
#define MAX_SESSIONS 100

// Add this prototype near the top of the file, with other function prototypes
void log_operation(const char* username, const char* operation, const char* details);

typedef struct {
    char username[50];
    int is_guest;
    int is_authenticated;
} UserSession;

// Function to handle authentication
UserSession handle_auth(int udp_socket, struct sockaddr_in server_a_addr, 
                       const char* username, const char* password) {
    UserSession session = {0};
    char buffer[BUFFER_SIZE];
    snprintf(buffer, BUFFER_SIZE, "%s %s", username, password);
    
    if (sendto(udp_socket, buffer, strlen(buffer), 0, 
               (struct sockaddr*)&server_a_addr, sizeof(server_a_addr)) < 0) {
        perror("Failed to send credentials to serverA");
        return session;
    }
    
    printf("Server M has sent authentication request to Server A\n");
    
    socklen_t server_len = sizeof(server_a_addr);
    int response_len = recvfrom(udp_socket, buffer, BUFFER_SIZE - 1, 0,
                               (struct sockaddr*)&server_a_addr, &server_len);
    if (response_len < 0) {
        perror("UDP receive from ServerA failed");
        return session;
    }
    
    printf("The main server has received the response from server A using UDP over %d\n", MAIN_TCP_PORT);
    
    buffer[response_len] = '\0';
    
    if (strcmp(buffer, "GUEST_AUTH_SUCCESS") == 0) {
        strcpy(session.username, username);
        session.is_guest = 1;
        session.is_authenticated = 1;
    } else if (strcmp(buffer, "MEMBER_AUTH_SUCCESS") == 0) {
        strcpy(session.username, username);
        session.is_guest = 0;
        session.is_authenticated = 1;
    }
    
    return session;
}

// Function to handle lookup requests
char* handle_lookup(int udp_socket, struct sockaddr_in server_r_addr, 
                   const char* target_username, const UserSession* session) {
    static char response[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];

    // First check if user is authenticated
    if (!session->is_authenticated) {
        return "Error: Please authenticate first";
    }
    
    // Send lookup request to Server R
    snprintf(buffer, BUFFER_SIZE, "LOOKUP %s %s", target_username, session->username);
    
    if (sendto(udp_socket, buffer, strlen(buffer), 0, 
               (struct sockaddr*)&server_r_addr, sizeof(server_r_addr)) < 0) {
        return "Error: Failed to send lookup request";
    }
    
    printf("The main server has sent the lookup request to server R.\n");
    
    // Receive response from Server R
    socklen_t server_len = sizeof(server_r_addr);
    int response_len = recvfrom(udp_socket, response, BUFFER_SIZE - 1, 0,
                               (struct sockaddr*)&server_r_addr, &server_len);
    if (response_len < 0) {
        return "Error: Failed to receive lookup response";
    }
    
    printf("The main server has received the response from server R using UDP over %d\n", MAIN_TCP_PORT);
    
    response[response_len] = '\0';
    
    // Log successful lookup operation
    if (strstr(response, "Error") == NULL) {
        log_operation(session->username, "LOOKUP", target_username);
    }
    
    return response;
}

// Function to handle push requests
char* handle_push(int udp_socket, struct sockaddr_in server_r_addr, 
                 const char* username, const char* filename, const UserSession* session) {
    static char response[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];

    if (!session->is_authenticated || session->is_guest) {
        return "Error: Only authenticated members can push files";
    }
    
    // Send push request to Server R
    snprintf(buffer, BUFFER_SIZE, "PUSH %s %s", username, filename);
    
    if (sendto(udp_socket, buffer, strlen(buffer), 0, 
               (struct sockaddr*)&server_r_addr, sizeof(server_r_addr)) < 0) {
        return "Error: Failed to send push request";
    }
    printf("The main server has sent the push request to server R.\n");
    
    // Receive response from Server R
    socklen_t server_len = sizeof(server_r_addr);
    int response_len = recvfrom(udp_socket, response, BUFFER_SIZE - 1, 0,
                               (struct sockaddr*)&server_r_addr, &server_len);
    if (response_len < 0) {
        return "Error: Failed to receive push response";
    }
    
    response[response_len] = '\0';
    
    // Print appropriate message based on response type
    if (strstr(response, "File exists") != NULL) {
        printf("The main server has received the response from server R using UDP over %d, asking for overwrite confirmation\n", MAIN_TCP_PORT);
    } else {
        printf("The main server has received the response from server R using UDP over %d\n", MAIN_TCP_PORT);
    }
    
    if (strstr(response, "successfully") != NULL) {
        log_operation(session->username, "PUSH", filename);
    }
    return response;
}

char* handle_deploy(int udp_socket, struct sockaddr_in server_d_addr, 
                   const char* username, const UserSession* session) {
    static char response[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];

    // Check if user is authenticated and is not a guest
    if (!session->is_authenticated || session->is_guest) {
        return "Error: Only authenticated members can deploy files";
    }
    
    // Send deploy request to Server D
    snprintf(buffer, BUFFER_SIZE, "DEPLOY %s", username);
    
    if (sendto(udp_socket, buffer, strlen(buffer), 0, 
               (struct sockaddr*)&server_d_addr, sizeof(server_d_addr)) < 0) {
        return "Error: Failed to send deploy request";
    }
    
    // Receive response from Server D
    socklen_t server_len = sizeof(server_d_addr);
    int response_len = recvfrom(udp_socket, response, BUFFER_SIZE - 1, 0,
                               (struct sockaddr*)&server_d_addr, &server_len);
    if (response_len < 0) {
        return "Error: Failed to receive deploy response";
    }
    
    response[response_len] = '\0';
    if (strstr(response, "successful") != NULL) {
        log_operation(session->username, "DEPLOY", "all files");
    }
    return response;
}

char* handle_remove(int udp_socket, struct sockaddr_in server_r_addr, 
                   const char* filename, const UserSession* session) {
    static char response[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];

    // Check if user is authenticated and is not a guest
    if (!session->is_authenticated || session->is_guest) {
        return "Error: Only authenticated members can remove files";
    }
    
    // Send remove request to Server R
    snprintf(buffer, BUFFER_SIZE, "REMOVE %s %s", session->username, filename);
    
    if (sendto(udp_socket, buffer, strlen(buffer), 0, 
               (struct sockaddr*)&server_r_addr, sizeof(server_r_addr)) < 0) {
        return "Error: Failed to send remove request";
    }
    
    // Receive response from Server R
    socklen_t server_len = sizeof(server_r_addr);
    int response_len = recvfrom(udp_socket, response, BUFFER_SIZE - 1, 0,
                               (struct sockaddr*)&server_r_addr, &server_len);
    if (response_len < 0) {
        return "Error: Failed to receive remove response";
    }
    
    response[response_len] = '\0';
    if (strstr(response, "successfully") != NULL) {
        log_operation(session->username, "REMOVE", filename);
    }
    return response;
}

char* handle_log_command(const UserSession* session) {
    static char response[BUFFER_SIZE * 10];  // Larger buffer for log history
    
    // Check if user is authenticated and is not a guest
    if (!session->is_authenticated || session->is_guest) {
        return "Error: Only authenticated members can view logs";
    }
    
    FILE* log_file = fopen("server_logs.txt", "r");
    if (!log_file) {
        return "Error: Could not access log history";
    }
    
    // Initialize response with header
    snprintf(response, BUFFER_SIZE * 10, "Operation history for user %s:\n", session->username);
    
    char line[BUFFER_SIZE];
    // Skip header if present
    fgets(line, sizeof(line), log_file);
    
    // Read each line and collect all logs for the user
    while (fgets(line, sizeof(line), log_file)) {
        char log_username[50];
        // Extract username from log line
        if (sscanf(line, "[%*[^]]] %s", log_username) == 1) {
            // If this log is for our user, append it to response
            if (strcmp(log_username, session->username) == 0) {
                strcat(response, line);
            }
        }
    }
    
    fclose(log_file);
    return response;
}

void log_operation(const char* username, const char* operation, const char* details) {
    FILE* log_file = fopen("server_logs.txt", "a");
    if (!log_file) {
        perror("Cannot open log file");
        return;
    }
    
    time_t now = time(NULL);
    char timestamp[26];
    ctime_r(&now, timestamp);
    timestamp[24] = '\0';  // Remove newline
    
    fprintf(log_file, "[%s] %s %s %s\n", timestamp, username, operation, details);
    fclose(log_file);
}

int main() {
    int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    
    if (tcp_socket < 0 || udp_socket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int reuse = 1;
    if (setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in tcp_addr, server_a_addr, server_r_addr, server_d_addr;
    
    memset(&tcp_addr, 0, sizeof(tcp_addr));
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_addr.s_addr = INADDR_ANY;
    tcp_addr.sin_port = htons(MAIN_TCP_PORT);

    memset(&server_a_addr, 0, sizeof(server_a_addr));
    server_a_addr.sin_family = AF_INET;
    server_a_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_a_addr.sin_port = htons(SERVER_A_PORT);

    memset(&server_r_addr, 0, sizeof(server_r_addr));
    server_r_addr.sin_family = AF_INET;
    server_r_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_r_addr.sin_port = htons(SERVER_R_PORT);

    memset(&server_d_addr, 0, sizeof(server_d_addr));
    server_d_addr.sin_family = AF_INET;
    server_d_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_d_addr.sin_port = htons(SERVER_D_PORT);

    if (bind(tcp_socket, (struct sockaddr*)&tcp_addr, sizeof(tcp_addr)) < 0) {
        perror("TCP bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(tcp_socket, 5) < 0) {
        perror("TCP listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server M is up and running using UDP on port %d\n", MAIN_TCP_PORT);

    // Keep track of authenticated sessions
    UserSession current_session = {0};

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        char buffer[BUFFER_SIZE];
        
        int client_socket = accept(tcp_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("TCP accept failed");
            continue;
        }

        // Keep connection alive for this client
        while (1) {
            int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
            if (bytes_received <= 0) {
                // Client disconnected or error
                break;
            }
            buffer[bytes_received] = '\0';

            char command[20], arg1[50], arg2[50];
            sscanf(buffer, "%s %s %s", command, arg1, arg2);

            if (strcmp(command, "AUTH") == 0) {
                // Print received credentials (with hidden password)
                char hidden_password[50];
                memset(hidden_password, '*', strlen(arg2));
                hidden_password[strlen(arg2)] = '\0';
                printf("Server M has received username %s and password %s\n", arg1, hidden_password);
                
                // Handle authentication
                current_session = handle_auth(udp_socket, server_a_addr, arg1, arg2);
                const char* response;
                
                if (current_session.is_guest && current_session.is_authenticated) {
                    response = "Authentication successful - Welcome guest! Note: Limited access rights applied";
                } else if (current_session.is_authenticated) {
                    response = "Authentication successful - Welcome member!";
                } else {
                    response = "Authentication failed - Invalid credentials";
                }
                
                send(client_socket, response, strlen(response), 0);
                printf("The main server has sent the response from server A to client using TCP over port %d\n", MAIN_TCP_PORT);
                
            } else if (strcmp(command, "LOOKUP") == 0) {
                // Print appropriate message based on user type
                if (current_session.is_guest) {
                    printf("The main server has received a lookup request from Guest to lookup %s's repository using TCP over port %d.\n", 
                           arg1, MAIN_TCP_PORT);
                } else {
                    printf("The main server has received a lookup request from %s to lookup %s's repository using TCP over port %d.\n", 
                           current_session.username, arg1, MAIN_TCP_PORT);
                }
                
                // Handle lookup request with current session
                char* lookup_response = handle_lookup(udp_socket, server_r_addr, arg1, &current_session);
                send(client_socket, lookup_response, strlen(lookup_response), 0);
                printf("The main server has sent the response to the client.\n");
            } else if (strcmp(command, "PUSH") == 0) {
                // Print received push request message
                printf("The main server has received a push request from %s, using TCP over port %d.\n", 
                       current_session.username, MAIN_TCP_PORT);
                
                // Handle push request with current session
                char* push_response = handle_push(udp_socket, server_r_addr, arg1, arg2, &current_session);
                send(client_socket, push_response, strlen(push_response), 0);
                
                // For non-overwrite responses
                if (strstr(push_response, "File exists") == NULL) {
                    printf("The main server has sent the response to the client.\n");
                } else {
                    // For overwrite confirmation requests
                    printf("The main server has sent the overwrite confirmation request to the client.\n");
                    
                    bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
                    if (bytes_received > 0) {
                        buffer[bytes_received] = '\0';
                        
                        printf("The main server has received the overwrite confirmation response from %s using TCP over port %d\n", 
                               current_session.username, MAIN_TCP_PORT);
                        
                        // Forward overwrite response to Server R
                        if (sendto(udp_socket, buffer, strlen(buffer), 0, 
                                  (struct sockaddr*)&server_r_addr, sizeof(server_r_addr)) < 0) {
                            send(client_socket, "Error: Failed to process overwrite response", 42, 0);
                            continue;
                        }
                        printf("The main server has sent the overwrite confirmation response to server R.\n");
                        
                        // Get final response from Server R
                        socklen_t server_len = sizeof(server_r_addr);
                        int response_len = recvfrom(udp_socket, buffer, BUFFER_SIZE - 1, 0,
                                              (struct sockaddr*)&server_r_addr, &server_len);
                        if (response_len > 0) {
                            buffer[response_len] = '\0';
                            printf("The main server has received the response from server R using UDP over %d\n", MAIN_TCP_PORT);
                            send(client_socket, buffer, strlen(buffer), 0);
                            printf("The main server has sent the response to the client.\n");
                        }
                    }
                }
            } else if (strcmp(command, "DEPLOY") == 0) {
                // Handle deploy request with current session
                char* deploy_response = handle_deploy(udp_socket, server_d_addr, arg1, &current_session);
                send(client_socket, deploy_response, strlen(deploy_response), 0);
            } else if (strcmp(command, "REMOVE") == 0) {
                // Handle remove request with current session
                char* remove_response = handle_remove(udp_socket, server_r_addr, arg1, &current_session);
                send(client_socket, remove_response, strlen(remove_response), 0);
            } else if (strcmp(command, "LOG") == 0) {
                char* log_response = handle_log_command(&current_session);
                send(client_socket, log_response, strlen(log_response), 0);
            }
        }

        close(client_socket);  // Only close when client disconnects
    }

    close(tcp_socket);
    close(udp_socket);
    return 0;
}