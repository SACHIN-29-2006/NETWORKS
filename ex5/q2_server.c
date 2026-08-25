#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define MAX_STUDENTS 5

#define STATUS_SUCCESS 200
#define STATUS_NOT_FOUND 404

// Request packet from client
typedef struct __attribute__((packed)) {
    char reg_no[20];
} StudentRequest;

// Response packet from server
typedef struct __attribute__((packed)) {
    int status_code;
    char reg_no[20];
    char name[50];
    char department[50];
    int semester;
    float cgpa;
    char message[100];
} StudentResponse;

// Student record for internal database
typedef struct {
    char reg_no[20];
    char name[50];
    char department[50];
    int semester;
    float cgpa;
} Student;

// Mock database initialization
Student database[MAX_STUDENTS] = {
    {"101", "Alex Mercer", "Computer Science", 6, 8.75f},
    {"102", "Beatrix Kiddo", "Information Technology", 4, 9.10f},
    {"103", "Charles Xavier", "Electrical Engineering", 8, 9.85f},
    {"104", "Diana Prince", "Mechanical Engineering", 2, 7.90f},
    {"105", "Evan Wright", "Cyber Security", 5, 8.42f}
};

int main() {
    int server_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("========================================================\n");
    printf(" Student Information Lookup Server Active on Port %d \n", PORT);
    printf("========================================================\n\n");

    while (1) {
        StudentRequest req;
        memset(&req, 0, sizeof(StudentRequest));

        int bytes = recvfrom(server_fd, &req, sizeof(StudentRequest), 0,
                             (struct sockaddr *)&client_addr, &addr_len);
        if (bytes < 0) continue;

        printf("[SERVER LOG] Received query for Reg No: '%s' from [%s:%d]\n",
               req.reg_no, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        StudentResponse resp;
        memset(&resp, 0, sizeof(StudentResponse));

        int found_index = -1;
        for (int i = 0; i < MAX_STUDENTS; i++) {
            if (strcasecmp(database[i].reg_no, req.reg_no) == 0) {
                found_index = i;
                break;
            }
        }

        if (found_index != -1) {
            resp.status_code = STATUS_SUCCESS;
            strncpy(resp.reg_no, database[found_index].reg_no, 19);
            strncpy(resp.name, database[found_index].name, 49);
            strncpy(resp.department, database[found_index].department, 49);
            resp.semester = database[found_index].semester;
            resp.cgpa = database[found_index].cgpa;
            strcpy(resp.message, "Student record found.");
        } else {
            resp.status_code = STATUS_NOT_FOUND;
            strncpy(resp.reg_no, req.reg_no, 19);
            snprintf(resp.message, sizeof(resp.message), "Error: Registration No '%s' not found in database.", req.reg_no);
        }

        sendto(server_fd, &resp, sizeof(StudentResponse), 0, (struct sockaddr *)&client_addr, addr_len);
    }

    close(server_fd);
    return 0;
}
