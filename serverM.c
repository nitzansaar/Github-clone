#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define MAIN_TCP_PORT 21690
#define SERVER_A_PORT 21693
#define SERVER_R_PORT 21694
#define BUFFER_SIZE 1024
#define MAX_SESSIONS 100

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
    
    socklen_t server_len = sizeof(server_a_addr);
    int response_len = recvfrom(udp_socket, buffer, BUFFER_SIZE - 1, 0,
                               (struct sockaddr*)&server_a_addr, &server_len);
    if (response_len < 0) {
        perror("UDP receive from ServerA failed");
        return session;
    }
    
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
    
    // Receive response from Server R
    socklen_t server_len = sizeof(server_r_addr);
    int response_len = recvfrom(udp_socket, response, BUFFER_SIZE - 1, 0,
                               (struct sockaddr*)&server_r_addr, &server_len);
    if (response_len < 0) {
        return "Error: Failed to receive lookup response";
    }
    
    response[response_len] = '\0';
    return response;
}

int main() {
    int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    
    if (tcp_socket < 0 || udp_socket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in tcp_addr, server_a_addr, server_r_addr;
    
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

    if (bind(tcp_socket, (struct sockaddr*)&tcp_addr, sizeof(tcp_addr)) < 0) {
        perror("TCP bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(tcp_socket, 5) < 0) {
        perror("TCP listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Main server is up and running\n");

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
                
            } else if (strcmp(command, "LOOKUP") == 0) {
                // Handle lookup request with current session
                char* lookup_response = handle_lookup(udp_socket, server_r_addr, arg1, &current_session);
                send(client_socket, lookup_response, strlen(lookup_response), 0);
            }
        }

        close(client_socket);  // Only close when client disconnects
    }

    close(tcp_socket);
    close(udp_socket);
    return 0;
}