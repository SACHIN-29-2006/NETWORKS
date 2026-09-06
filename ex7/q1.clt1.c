#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 2048

typedef struct {
    int action; // 1: View, 2: Book, 3: Cancel, 4: Check
    int client_id;
    int room_number;
    int res_id;
} UDPRequest;

int main() {
    int sock;
    struct sockaddr_in serv_addr;
    socklen_t addr_len = sizeof(serv_addr);
    char response[BUFFER_SIZE];
    int client_id, choice;

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("\n UDP Socket Creation Error \n");
        return -1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    printf("Enter Your Client ID (e.g., 501, 502): ");
    if (scanf("%d", &client_id) != 1) return -1;

    while (1) {
        printf("\n================ HOTEL RESERVATION MENU ================\n");
        printf("1. View Room Availability\n");
        printf("2. Book a Room\n");
        printf("3. Cancel a Reservation\n");
        printf("4. Check Booking Details\n");
        printf("5. Exit\n");
        printf("Enter Choice (1-5): ");

        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            continue;
        }

        if (choice == 5) {
            printf("[-] Exiting client program. Goodbye!\n");
            break;
        }

        UDPRequest req;
        memset(&req, 0, sizeof(UDPRequest));
        req.action = choice;
        req.client_id = client_id;

        if (choice == 2) {
            printf("Enter Room Number to Book (e.g., 101, 102, 201...): ");
            scanf("%d", &req.room_number);
        } else if (choice == 3) {
            printf("Enter Reservation ID to Cancel: ");
            scanf("%d", &req.res_id);
        }

        // Send request packet over UDP
        sendto(sock, &req, sizeof(UDPRequest), 0, (struct sockaddr*)&serv_addr, addr_len);

        // Receive response over UDP
        memset(response, 0, BUFFER_SIZE);
        recvfrom(sock, response, BUFFER_SIZE - 1, 0, (struct sockaddr*)&serv_addr, &addr_len);

        printf("\n%s\n", response);
    }

    close(sock);
    return 0;
}
