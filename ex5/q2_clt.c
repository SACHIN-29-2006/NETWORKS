#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define TIMEOUT_SEC 3

#define STATUS_SUCCESS 200
#define STATUS_NOT_FOUND 404

typedef struct __attribute__((packed)) {
    char reg_no[20];
} StudentRequest;

typedef struct __attribute__((packed)) {
    int status_code;
    char reg_no[20];
    char name[50];
    char department[50];
    int semester;
    float cgpa;
    char message[100];
} StudentResponse;

int main() {
    int client_fd;
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);
    struct timeval tv;

    client_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (client_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure receive timeout
    tv.tv_sec = TIMEOUT_SEC;
    tv.tv_usec = 0;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    char input_buf[64];

    while (1) {
        printf("\n--------------------------------------------------------\n");
        printf("Enter Student Registration No (or type 'exit' to quit): ");
        if (!fgets(input_buf, sizeof(input_buf), stdin)) break;

        // Strip trailing newline
        input_buf[strcspn(input_buf, "\r\n")] = 0;

        if (strlen(input_buf) == 0) {
            printf("[CLIENT ERROR] Input cannot be empty. Try again.\n");
            continue;
        }

        if (strcasecmp(input_buf, "exit") == 0) {
            printf("Exiting Student Lookup Client.\n");
            break;
        }

        StudentRequest req;
        memset(&req, 0, sizeof(StudentRequest));
        strncpy(req.reg_no, input_buf, 19);

        printf("[CLIENT] Requesting details for Registration No: '%s'...\n", req.reg_no);
        sendto(client_fd, &req, sizeof(StudentRequest), 0, (struct sockaddr *)&server_addr, addr_len);

        StudentResponse resp;
        memset(&resp, 0, sizeof(StudentResponse));

        int bytes = recvfrom(client_fd, &resp, sizeof(StudentResponse), 0, NULL, NULL);

        if (bytes > 0) {
            if (resp.status_code == STATUS_SUCCESS) {
                printf("\n========================================================\n");
                printf("                STUDENT DETAILS FOUND                   \n");
                printf("========================================================\n");
                printf(" Registration No : %s\n", resp.reg_no);
                printf(" Name            : %s\n", resp.name);
                printf(" Department      : %s\n", resp.department);
                printf(" Semester        : %d\n", resp.semester);
                printf(" CGPA            : %.2f\n", resp.cgpa);
                printf("========================================================\n");
            } else {
                printf("\n[SERVER RESPONSE] %s\n", resp.message);
            }
        } else {
            printf("\n[CLIENT ERROR] Request timed out. Server failed to respond.\n");
        }
    }

    close(client_fd);
    return 0;
}
