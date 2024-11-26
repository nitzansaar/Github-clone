#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ctype.h>
#include <time.h>
#include <sys/select.h>
#include <pthread.h>

#define MAIN_TCP_PORT 21690
#define GUEST_TCP_PORT 21691
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

typedef struct {
    int socket;
    struct sockaddr_in address;
    UserSession session;
} ClientConnection;

// Add mutex for thread-safe operations
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

// Add near the top of the file with other global definitions
typedef struct {
    int udp_socket;
    struct sockaddr_in server_a_addr;
    struct sockaddr_in server_r_addr;
    struct sockaddr_in server_d_addr;
} ServerResources;

// Global instance
ServerResources server_resources;

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
    pthread_mutex_lock(&log_mutex);
    
    FILE* log_file = fopen("server_logs.txt", "a");
    if (!log_file) {
        perror("Cannot open log file");
        pthread_mutex_unlock(&log_mutex);
        return;
    }
    
    time_t now = time(NULL);
    char timestamp[26];
    ctime_r(&now, timestamp);
    timestamp[24] = '\0';  // Remove newline
    
    fprintf(log_file, "[%s] %s %s %s\n", timestamp, username, operation, details);
    fclose(log_file);
    
    pthread_mutex_unlock(&log_mutex);
}

void* handle_client(void* arg) {
    ClientConnection* conn = (ClientConnection*)arg;
    char buffer[BUFFER_SIZE];
    
    while (1) {
        int bytes_received = recv(conn->socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            break;  // Client disconnected
        }
        buffer[bytes_received] = '\0';

        char command[20], arg1[50], arg2[50];
        sscanf(buffer, "%s %s %s", command, arg1, arg2);

        char* response = NULL;

        if (strcmp(command, "AUTH") == 0) {
            pthread_mutex_lock(&log_mutex);
            printf("Server M has received username %s and password %s\n", 
                   arg1, "********");
            pthread_mutex_unlock(&log_mutex);
            
            conn->session = handle_auth(server_resources.udp_socket, 
                                      server_resources.server_a_addr, 
                                      arg1, arg2);
            
            if (conn->session.is_guest && conn->session.is_authenticated) {
                response = "Authentication successful - Welcome guest! Note: Limited access rights applied";
            } else if (conn->session.is_authenticated) {
                response = "Authentication successful - Welcome member!";
            } else {
                response = "Authentication failed - Invalid credentials";
            }
        }
        else if (strcmp(command, "LOOKUP") == 0) {
            response = handle_lookup(server_resources.udp_socket, 
                                   server_resources.server_r_addr,
                                   arg1, &conn->session);
        }
        else if (strcmp(command, "PUSH") == 0) {
            response = handle_push(server_resources.udp_socket,
                                 server_resources.server_r_addr,
                                 conn->session.username, arg1, &conn->session);
        }
        else if (strcmp(command, "DEPLOY") == 0) {
            response = handle_deploy(server_resources.udp_socket,
                                   server_resources.server_d_addr,
                                   conn->session.username, &conn->session);
        }
        else if (strcmp(command, "REMOVE") == 0) {
            response = handle_remove(server_resources.udp_socket,
                                   server_resources.server_r_addr,
                                   arg1, &conn->session);
        }
        else if (strcmp(command, "LOG") == 0) {
            response = handle_log_command(&conn->session);
        }
        else {
            response = "Invalid command";
        }

        // Send response back to client
        if (response) {
            send(conn->socket, response, strlen(response), 0);
            pthread_mutex_lock(&log_mutex);
            printf("The main server has sent the response to client\n");
            pthread_mutex_unlock(&log_mutex);
        }
    }

    close(conn->socket);
    free(conn);
    return NULL;
}

int main() {
    int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    int tcp_guest_socket = socket(AF_INET, SOCK_STREAM, 0);
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    
    if (tcp_socket < 0 || udp_socket < 0 || tcp_guest_socket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int reuse = 1;
    if (setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0 ||
        setsockopt(tcp_guest_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in tcp_addr, server_a_addr, server_r_addr, server_d_addr, tcp_guest_addr;
    
    memset(&tcp_addr, 0, sizeof(tcp_addr));
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_addr.s_addr = INADDR_ANY;
    tcp_addr.sin_port = htons(MAIN_TCP_PORT);

    memset(&tcp_guest_addr, 0, sizeof(tcp_guest_addr));
    tcp_guest_addr.sin_family = AF_INET;
    tcp_guest_addr.sin_addr.s_addr = INADDR_ANY;
    tcp_guest_addr.sin_port = htons(GUEST_TCP_PORT);


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
        perror("Main TCP bind failed");
        exit(EXIT_FAILURE);
    }

    if (bind(tcp_guest_socket, (struct sockaddr*)&tcp_guest_addr, sizeof(tcp_guest_addr)) < 0) {
        perror("Guest TCP bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(tcp_socket, 5) < 0 || listen(tcp_guest_socket, 5) < 0) {
        perror("TCP listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server M is up and running using UDP on port %d and TCP on ports %d, %d\n", 
           MAIN_TCP_PORT, MAIN_TCP_PORT, GUEST_TCP_PORT);

    // Initialize server resources
    server_resources.udp_socket = udp_socket;
    server_resources.server_a_addr = server_a_addr;
    server_resources.server_r_addr = server_r_addr;
    server_resources.server_d_addr = server_d_addr;

    // Keep track of authenticated sessions
    UserSession current_session = {0};

    while (1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(tcp_socket, &readfds);
        FD_SET(tcp_guest_socket, &readfds);
        
        int max_fd = (tcp_socket > tcp_guest_socket) ? tcp_socket : tcp_guest_socket;
        
        // Use timeout to make select non-blocking
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;  // 100ms timeout
        
        int activity = select(max_fd + 1, &readfds, NULL, NULL, &timeout);
        if (activity < 0) {
            perror("select failed");
            continue;
        }
        
        if (activity == 0) continue;  // Timeout, no new connections
        
        if (FD_ISSET(tcp_socket, &readfds) || FD_ISSET(tcp_guest_socket, &readfds)) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_socket;
            
            if (FD_ISSET(tcp_socket, &readfds)) {
                client_socket = accept(tcp_socket, (struct sockaddr*)&client_addr, &client_len);
            } else {
                client_socket = accept(tcp_guest_socket, (struct sockaddr*)&client_addr, &client_len);
            }

            if (client_socket < 0) {
                perror("TCP accept failed");
                continue;
            }

            // Create new client connection
            ClientConnection* conn = malloc(sizeof(ClientConnection));
            conn->socket = client_socket;
            conn->address = client_addr;
            memset(&conn->session, 0, sizeof(UserSession));

            // Create thread to handle this client
            pthread_t thread_id;
            if (pthread_create(&thread_id, NULL, handle_client, (void*)conn) != 0) {
                perror("Failed to create thread");
                close(client_socket);
                free(conn);
                continue;
            }
            
            // Detach thread to automatically clean up when it exits
            pthread_detach(thread_id);
        }
    }

    close(tcp_socket);
    close(tcp_guest_socket);
    close(udp_socket);
    return 0;
}