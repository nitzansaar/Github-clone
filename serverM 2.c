#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define MAIN_TCP_PORT 21690  // TCP port client
#define SERVER_A_PORT 21693  // UDP port for ServerA
#define BUFFER_SIZE 1024


int main() {
    // set up server that communicates with clients via TCP
    // and communicates with servers using UDP

    //create TCP socket
    int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_socket < 0) {
        perror("error setting up tcp socket");
        exit(EXIT_FAILURE);
    }

    //create UDP socket
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket < 0) {
        perror("error setting up udp socket");
        exit(EXIT_FAILURE);
    }

    //configure tcp address for client communication
    struct sockaddr_in tcp_addr;
    memset(&tcp_addr, 0, sizeof(tcp_addr));
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_addr.s_addr = INADDR_ANY;
    tcp_addr.sin_port = htons(MAIN_TCP_PORT);

    //configure udp address for server communication
    struct sockaddr_in server_a_addr;
    memset(&server_a_addr, 0, sizeof(server_a_addr));
    server_a_addr.sin_family = AF_INET;
    server_a_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_a_addr.sin_port = htons(SERVER_A_PORT);

    //bind TCP socket
    if (bind(tcp_socket, (struct sockaddr*)&tcp_addr, sizeof(tcp_addr)) < 0) {
        perror("TCP bind failed");
        exit(EXIT_FAILURE);
    }

    //listen for TCP connections
    if (listen(tcp_socket, 5) < 0) {
        perror("TCP listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Main server is up and running\n");

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_socket = accept(tcp_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("TCP accept failed");
            continue; // keep going if the connection fails
        }

        printf("Main server received a request from client\n");

        //receive credential from client
        char buffer[BUFFER_SIZE];
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received < 0) {
            perror("TCP receive failed");
            close(client_socket);
            continue;
        }

        buffer[bytes_received] = '\0';

        char username[50], password[50];
        sscanf(buffer, "%s %s", username, password);
        printf("received username {%s} and password {%s}", username, password);

        //send credentials to serverA
        if (sendto(udp_socket, buffer, strlen(buffer), 0, (struct sockaddr*)&server_a_addr, sizeof(server_a_addr)) < 0) {
            perror("Failed to send credentials to serverA");
            close(client_socket);
            continue;
        }
        
        printf("ServerM sent authentication request to serverA\n");

        //recieve response from serverA
        char response[BUFFER_SIZE];
        socklen_t server_a_len = sizeof(server_a_addr);
        int response_len = recvfrom(udp_socket, response, BUFFER_SIZE - 1, 0,
                                  (struct sockaddr*)&server_a_addr, &server_a_len);
        if (response_len < 0) {
            perror("UDP receive from ServerA failed");
            close(client_socket);
            continue;
        }
        response[response_len] = '\0';
        printf("Main server received the response from ServerA: %s\n", response);

        // Send response back to client
        if (send(client_socket, response, strlen(response), 0) < 0) {
            perror("TCP send to client failed");
        }
        printf("Main server sent the authentication result to client\n");

        close(client_socket);
    }
    close(tcp_socket);
    close(udp_socket);
    return 0;

}