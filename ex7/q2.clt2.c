#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int sock;
    struct sockaddr_in serv_addr;
    socklen_t addr_len = sizeof(serv_addr);
    char buffer[BUFFER_SIZE];
    char username[32];

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    printf("Enter your Chat Username: ");
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\r\n")] = 0;

    // Send JOIN command payload to Server
    snprintf(buffer, sizeof(buffer), "JOIN %s", username);
    sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr *)&serv_addr, addr_len);

    printf("[+] Joined Chat Room! Type messages below (type 'EXIT' to disconnect):\n\n");

    fd_set read_fds;

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds); // Monitor keyboard input
        FD_SET(sock, &read_fds);         // Monitor incoming UDP broadcast messages

        int activity = select(sock + 1, &read_fds, NULL, NULL, NULL);

        if (activity < 0) break;

        // 1. Incoming Message From Server
        if (FD_ISSET(sock, &read_fds)) {
            memset(buffer, 0, BUFFER_SIZE);
            int bytes = recvfrom(sock, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr *)&serv_addr, &addr_len);
            if (bytes > 0) {
                printf("\r%s\n> ", buffer);
                fflush(stdout);
            }
        }

        // 2. User Input From Terminal
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            memset(buffer, 0, BUFFER_SIZE);
            if (fgets(buffer, BUFFER_SIZE, stdin) != NULL) {
                buffer[strcspn(buffer, "\r\n")] = 0;

                sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr *)&serv_addr, addr_len);

                if (strncmp(buffer, "EXIT", 4) == 0) {
                    printf("[-] Disconnecting from chat. Goodbye!\n");
                    break;
                }
                printf("> ");
                fflush(stdout);
            }
        }
    }

    close(sock);
    return 0;
}
