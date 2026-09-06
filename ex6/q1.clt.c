#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 2048

void receive_stream(int sock) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    
    while (recv(sock, buffer, BUFFER_SIZE - 1, 0) > 0) {
        char *eof_ptr = strstr(buffer, "EOF");
        if (eof_ptr) {
            *eof_ptr = '\0';
            printf("%s", buffer);
            break;
        }
        printf("%s", buffer);
        memset(buffer, 0, BUFFER_SIZE);
    }
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    int choice;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\n Invalid address \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\n Connection Failed! Ensure server is running.\n");
        return -1;
    }

    printf("[+] Connected to Hospital Management Server!\n");

    while (1) {
        printf("\n================ HOSPITAL MENU ================\n");
        printf("1. Patient Registration\n");
        printf("2. Search Doctor Availability & Slots\n");
        printf("3. Book Appointment\n");
        printf("4. Cancel Appointment\n");
        printf("5. View Appointment History\n");
        printf("6. Exit Session\n");
        printf("Enter Choice (1-6): ");

        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            continue;
        }
        getchar(); // Clear trailing newline

        send(sock, &choice, sizeof(int), 0);

        if (choice == 1) {
            char name[50], phone[15], response[BUFFER_SIZE] = {0};
            printf("Enter Full Name: ");
            fgets(name, sizeof(name), stdin);
            printf("Enter Phone Number: ");
            fgets(phone, sizeof(phone), stdin);

            send(sock, name, sizeof(name), 0);
            send(sock, phone, sizeof(phone), 0);

            recv(sock, response, sizeof(response) - 1, 0);
            printf("\n%s\n", response);

        } else if (choice == 2) {
            receive_stream(sock);

        } else if (choice == 3) {
            int pid, doc_id, slot_num;
            char response[BUFFER_SIZE] = {0};

            printf("Enter Patient ID: ");
            scanf("%d", &pid);
            printf("Enter Doctor ID (e.g., 101, 102, 103): ");
            scanf("%d", &doc_id);
            printf("Enter Desired Slot Number (e.g., 1, 2, 3, 4): ");
            scanf("%d", &slot_num);
            
            while (getchar() != '\n'); // Clear stdin buffer

            send(sock, &pid, sizeof(int), 0);
            send(sock, &doc_id, sizeof(int), 0);
            send(sock, &slot_num, sizeof(int), 0);

            recv(sock, response, sizeof(response) - 1, 0);
            printf("\n%s\n", response);

        } else if (choice == 4) {
            int appt_id;
            char response[BUFFER_SIZE] = {0};

            printf("Enter Appointment ID to Cancel: ");
            scanf("%d", &appt_id);
            while (getchar() != '\n'); // Clear stdin buffer

            send(sock, &appt_id, sizeof(int), 0);

            recv(sock, response, sizeof(response) - 1, 0);
            printf("\n%s\n", response);

        } else if (choice == 5) {
            int pid;
            printf("Enter Patient ID: ");
            scanf("%d", &pid);
            while (getchar() != '\n'); // Clear stdin buffer

            send(sock, &pid, sizeof(int), 0);
            receive_stream(sock);

        } else if (choice == 6) {
            printf("[-] Ending session. Goodbye!\n");
            break;
        } else {
            printf("[-] Invalid Option!\n");
        }
    }

    close(sock);
    return 0;
}
