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

    printf("[+] Connected to College Library Management Server!\n");

    while (1) {
        printf("\n================ LIBRARY MENU ================\n");
        printf("1. Search Books (Title / Author)\n");
        printf("2. Check Entire Catalog Availability\n");
        printf("3. Issue Book\n");
        printf("4. Return Book\n");
        printf("5. View Issued Books History\n");
        printf("6. Exit Session\n");
        printf("Enter Choice (1-6): ");

        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            continue;
        }
        getchar(); // Clear trailing newline

        send(sock, &choice, sizeof(int), 0);

        if (choice == 1) {
            char query[100];
            printf("Enter Title or Author search term: ");
            fgets(query, sizeof(query), stdin);

            send(sock, query, sizeof(query), 0);
            receive_stream(sock);

        } else if (choice == 2) {
            receive_stream(sock);

        } else if (choice == 3) {
            int sid, bid;
            char response[BUFFER_SIZE] = {0};

            printf("Enter Student ID: ");
            scanf("%d", &sid);
            printf("Enter Book ID to Issue (e.g., 101, 102...): ");
            scanf("%d", &bid);
            while (getchar() != '\n'); // Clear stdin buffer

            send(sock, &sid, sizeof(int), 0);
            send(sock, &bid, sizeof(int), 0);

            recv(sock, response, sizeof(response) - 1, 0);
            printf("\n%s\n", response);

        } else if (choice == 4) {
            int issue_id;
            char response[BUFFER_SIZE] = {0};

            printf("Enter Transaction ID to Return Book: ");
            scanf("%d", &issue_id);
            while (getchar() != '\n'); // Clear stdin buffer

            send(sock, &issue_id, sizeof(int), 0);

            recv(sock, response, sizeof(response) - 1, 0);
            printf("\n%s\n", response);

        } else if (choice == 5) {
            int sid;
            printf("Enter Student ID: ");
            scanf("%d", &sid);
            while (getchar() != '\n'); // Clear stdin buffer

            send(sock, &sid, sizeof(int), 0);
            receive_stream(sock);

        } else if (choice == 6) {
            printf("[-] Ending library session. Goodbye!\n");
            break;
        } else {
            printf("[-] Invalid Option!\n");
        }
    }

    close(sock);
    return 0;
}
