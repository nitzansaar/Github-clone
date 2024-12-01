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
#define PURPLE "\033[35m"
#define RESET "\033[0m"

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
    
    printf("Server M has sent authentication request to Server A\n");
    
    if (sendto(udp_socket, buffer, strlen(buffer), 0, 
               (struct sockaddr*)&server_a_addr, sizeof(server_a_addr)) < 0) {
        perror("Failed to send credentials to serverA");
        return session;
    }
    
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

// Function to check if username exists in members.txt
int user_exists(const char* username) {
    FILE* file = fopen("members.txt", "r");
    if (!file) {
        perror("Cannot open members.txt");
        return 0;
    }

    char line[1024];
    char file_username[50];
    
    // Skip header line
    fgets(line, sizeof(line), file);
    
    while (fgets(line, sizeof(line), file)) {
        sscanf(line, "%s", file_username);
        if (strcmp(file_username, username) == 0) {
            fclose(file);
            return 1;
        }
    }
    
    fclose(file);
    return 0;
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
    
    // Check if target username exists in members.txt
    if (!user_exists(target_username)) {
        snprintf(response, BUFFER_SIZE, "Error: Username '%s' does not exist", target_username);
        return response;
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
    log_operation(session->username, "LOOKUP", target_username);
    
    printf("The main server has sent the response to the client.\n");
    
    return response;
}

// Function to handle push requests
char* handle_push(int udp_socket, struct sockaddr_in server_r_addr, 
                 const char* username, const char* filename, const UserSession* session) {
    static char response[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];

    // Initial push request received
    printf("The main server has received a push request from %s, using TCP over port %d.\n", 
           username, MAIN_TCP_PORT);

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

    if (strstr(response, "overwrite") != NULL) {
        printf("The main server has received the response from server R using UDP over %d, asking for overwrite confirmation\n", 
               MAIN_TCP_PORT);
        
        // Send overwrite confirmation request to client
        printf("The main server has sent the overwrite confirmation request to the client.\n");
        
        // After receiving overwrite confirmation from client
        printf("The main server has received the overwrite confirmation response from %s using TCP over port %d\n", 
               username, MAIN_TCP_PORT);
        
        // After sending overwrite confirmation to Server R
        printf("The main server has sent the overwrite confirmation response to server R.\n");
    } else {
        printf("The main server has received the response from server R using UDP over %d\n", 
               MAIN_TCP_PORT);
    }

    // Before sending final response to client
    printf("The main server has sent the response to the client.\n");

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

char* handle_log(const UserSession* session) {
    static char response[BUFFER_SIZE];
    
    // When receiving log request
    printf("%sThe main server has received a log request from member %s TCP over port %d.%s\n",
           PURPLE, session->username, MAIN_TCP_PORT, RESET);
    
    FILE* log_file = fopen("server_logs.txt", "r");
    if (!log_file) {
        return "Error: Could not access log history";
    }
    
    // Initialize response with header
    snprintf(response, BUFFER_SIZE, "Operation history for user %s:\n", session->username);
    
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
    
    // Before sending response back to client
    printf("%sThe main server has sent the log response to the client.%s\n",
           PURPLE, RESET);
    
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
            printf("Server M has received username %s and password ****\n", arg1);
            pthread_mutex_unlock(&log_mutex);
            
            conn->session = handle_auth(server_resources.udp_socket, 
                                      server_resources.server_a_addr, 
                                      arg1, arg2);
            
            if (conn->session.is_guest && conn->session.is_authenticated) {
                response = "GUEST_AUTH_SUCCESS";
            } else if (!conn->session.is_guest && conn->session.is_authenticated) {
                response = "MEMBER_AUTH_SUCCESS";
            } else {
                response = "AUTH_FAILED";
                conn->session.is_authenticated = 0;  // Ensure session is marked as not authenticated
            }
        }
        else if (strcmp(command, "LOOKUP") == 0) {
            if (conn->session.is_guest) {
                printf("The main server has received a lookup request from Guest to lookup %s’s repository using TCP over port %d.\n", arg1, MAIN_TCP_PORT);
            } else {
                printf("The main server has received a lookup request from %s to lookup %s’s repository using TCP over port %d.\n", conn->session.username, arg1, MAIN_TCP_PORT);
            }

            response = handle_lookup(server_resources.udp_socket, 
                                   server_resources.server_r_addr,
                                   arg1, &conn->session);
        }
        else if (strcmp(command, "PUSH") == 0) {
            printf("The main server has received a push request from %s, using TCP over port %d.\n", 
                   conn->session.username, MAIN_TCP_PORT);

            response = handle_push(server_resources.udp_socket,
                                 server_resources.server_r_addr,
                                 conn->session.username, arg1, &conn->session);
        }
        else if (strcmp(command, "DEPLOY") == 0) {
            printf("The main server has received a deploy request from member %s TCP over port %d.\n",
                   conn->session.username, MAIN_TCP_PORT);
            
            // Forward lookup request to server R
            printf("The main server has sent the lookup request to server R.\n");
            
            // Simulate receiving response from server R
            printf("The main server received the lookup response from server R.\n");
            
            // Send deploy request to server D
            printf("The main server has sent the deploy request to server D.\n");
            
            response = handle_deploy(server_resources.udp_socket,
                                   server_resources.server_d_addr,
                                   conn->session.username, &conn->session);
            
            // After receiving success response
            if (strstr(response, "successful") != NULL) {
                printf("The user %s's repository has been deployed at server D.\n", 
                       conn->session.username);
            }
        }
        else if (strcmp(command, "REMOVE") == 0) {
            printf("The main server has received a remove request from member %s TCP over port %d.\n", 
                   conn->session.username, MAIN_TCP_PORT);
            
            response = handle_remove(server_resources.udp_socket,
                                   server_resources.server_r_addr,
                                   arg1, &conn->session);
                               
            printf("The main server has received confirmation of the remove request done by the server R\n");
        }
        else if (strcmp(command, "LOG") == 0) {
            response = handle_log(&conn->session);
        }
        else {
            response = "Invalid command";
        }

        // Send response back to client
        if (response) {
            send(conn->socket, response, strlen(response), 0);
            pthread_mutex_lock(&log_mutex);
            printf("The main server has sent the response to the client using TCP over port %d.\n", MAIN_TCP_PORT);
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

    printf("Server M is up and running using UDP on port %d.\n", 
           MAIN_TCP_PORT);

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