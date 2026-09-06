#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <time.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 50
#define CLIENT_TIMEOUT 30 // Seconds before an inactive client is purged

typedef struct {
    struct sockaddr_in addr;
    char username[32];
    time_t last_active;
    int is_active;
} ClientSession;

ClientSession clients[MAX_CLIENTS];
int client_count = 0;

// Utility to check if two socket addresses match
int same_address(struct sockaddr_in *a, struct sockaddr_in *b) {
    return (a->sin_addr.s_addr == b->sin_addr.s_addr && a->sin_port == b->sin_port);
}

// Broadcast message to all active clients EXCEPT the sender
void broadcast_message(int server_sock, const char *msg, struct sockaddr_in *sender_addr) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].is_active) {
            // Do not echo back to the sender
            if (sender_addr == NULL || !same_address(&clients[i].addr, sender_addr)) {
                sendto(server_sock, msg, strlen(msg), 0,
                       (struct sockaddr *)&clients[i].addr, sizeof(clients[i].addr));
            }
        }
    }
}

// Add or update client session
void update_client_session(int server_sock, struct sockaddr_in *cli_addr, const char *username) {
    time_t now = time(NULL);

    // Check if client already exists in active session list
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].is_active && same_address(&clients[i].addr, cli_addr)) {
            clients[i].last_active = now; // Refresh activity timestamp
            return;
        }
    }

    // Add new client to active list
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].is_active) {
            clients[i].addr = *cli_addr;
            strncpy(clients[i].username, username, 31);
            clients[i].last_active = now;
            clients[i].is_active = 1;
            client_count++;

            char announce[BUFFER_SIZE];
            snprintf(announce, sizeof(announce), "[SYSTEM NOTIFICATION] %s joined the chat room!", username);
            printf("[+] %s (%s:%d) connected.\n", username, inet_ntoa(cli_addr->sin_addr), ntohs(cli_addr->sin_port));
            
            // Broadcast join notification to everyone
            broadcast_message(server_sock, announce, cli_addr);
            
            // Welcome message to joining client
            char welcome[] = "[SYSTEM] Connected to UDP Chat Broadcast Room!";
            sendto(server_sock, welcome, strlen(welcome), 0, (struct sockaddr *)cli_addr, sizeof(*cli_addr));
            return;
        }
    }
}

// Remove client on explicit exit or disconnection timeout
void remove_client(int server_sock, struct sockaddr_in *cli_addr, const char *reason) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].is_active && same_address(&clients[i].addr, cli_addr)) {
            clients[i].is_active = 0;
            client_count--;

            char announce[BUFFER_SIZE];
            snprintf(announce, sizeof(announce), "[SYSTEM NOTIFICATION] %s left the chat (%s).", clients[i].username, reason);
            printf("[-] %s disconnected (%s).\n", clients[i].username, reason);

            broadcast_message(server_sock, announce, NULL);
            return;
        }
    }
}

// Periodically check and prune inactive clients
void check_timeouts(int server_sock) {
    time_t now = time(NULL);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].is_active && (now - clients[i].last_active) > CLIENT_TIMEOUT) {
            remove_client(server_sock, &clients[i].addr, "Inactivity Timeout");
        }
    }
}

int main() {
    int server_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    for (int i = 0; i < MAX_CLIENTS; i++) clients[i].is_active = 0;

    if ((server_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("[+] Concurrent UDP Chat Notification Server running on port %d...\n", PORT);

    fd_set read_fds;
    struct timeval tv;

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(server_sock, &read_fds);

        // Non-blocking timeout select loop (checks every 2 seconds)
        tv.tv_sec = 2;
        tv.tv_usec = 0;

        int activity = select(server_sock + 1, &read_fds, NULL, NULL, &tv);

        if (activity > 0 && FD_ISSET(server_sock, &read_fds)) {
            memset(buffer, 0, BUFFER_SIZE);
            int bytes_read = recvfrom(server_sock, buffer, BUFFER_SIZE - 1, 0,
                                      (struct sockaddr *)&client_addr, &addr_len);

            if (bytes_read > 0) {
                buffer[strcspn(buffer, "\r\n")] = 0;

                // Handle JOIN Command: JOIN <Username>
                if (strncmp(buffer, "JOIN ", 5) == 0) {
                    char *username = buffer + 5;
                    update_client_session(server_sock, &client_addr, username);

                // Handle EXIT Command: EXIT
                } else if (strncmp(buffer, "EXIT", 4) == 0) {
                    remove_client(server_sock, &client_addr, "User Disconnected");

                // Handle Broadcast Text Message
                } else {
                    update_client_session(server_sock, &client_addr, "Unknown");

                    // Find username
                    char username[32] = "Anonymous";
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (clients[i].is_active && same_address(&clients[i].addr, &client_addr)) {
                            strncpy(username, clients[i].username, 31);
                            break;
                        }
                    }

                    char formatted_msg[BUFFER_SIZE + 50];
                    snprintf(formatted_msg, sizeof(formatted_msg), "[%s]: %s", username, buffer);
                    printf("%s\n", formatted_msg);

                    // Relay message out to all other clients concurrently
                    broadcast_message(server_sock, formatted_msg, &client_addr);
                }
            }
        }

        // Run routine active session housekeeping
        check_timeouts(server_sock);
    }

    close(server_sock);
    return 0;
}
