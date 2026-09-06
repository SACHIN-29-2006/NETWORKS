#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define PORT 8080
#define BUFFER_SIZE 2048
#define MAX_BOOKS 5
#define MAX_RECORDS 100

typedef struct {
    int book_id;
    char title[100];
    char author[50];
    char category[30];
    int is_available; // 1 = Available, 0 = Issued
} Book;

typedef struct {
    int issue_id;
    int student_id;
    int book_id;
    char status[20]; // "ISSUED", "RETURNED"
} IssueRecord;

// Global Library Records
Book catalog[MAX_BOOKS];
IssueRecord records[MAX_RECORDS];
int record_count = 0;

void log_transaction(const char *msg) {
    FILE *log_file = fopen("library_log.txt", "a");
    if (log_file) {
        time_t now = time(NULL);
        char *timestamp = ctime(&now);
        timestamp[strcspn(timestamp, "\n")] = 0;
        fprintf(log_file, "[%s] %s\n", timestamp, msg);
        fclose(log_file);
    }
}

void init_catalog() {
    catalog[0] = (Book){101, "Computer Networks", "Andrew Tanenbaum", "Networking", 1};
    catalog[1] = (Book){102, "Operating System Concepts", "Silberschatz", "OS", 1};
    catalog[2] = (Book){103, "Database System Concepts", "Korth", "DBMS", 1};
    catalog[3] = (Book){104, "Data Structures in C", "Reema Thareja", "Algorithms", 1};
    catalog[4] = (Book){105, "Core Java Programming", "Cay Horstmann", "Java", 1};

    log_transaction("Library catalog initialized.");
}

void handle_search_books(int sock) {
    char query[100] = {0};
    char response[BUFFER_SIZE] = "=== SEARCH RESULTS ===\n";
    char temp[256];

    recv(sock, query, sizeof(query) - 1, 0);
    query[strcspn(query, "\r\n")] = 0;

    int found = 0;
    for (int i = 0; i < MAX_BOOKS; i++) {
        // Case-insensitive substring search for Title or Author
        if (strcasestr(catalog[i].title, query) != NULL || strcasestr(catalog[i].author, query) != NULL) {
            found = 1;
            snprintf(temp, sizeof(temp), "ID: %d | Title: %s | Author: %s | Category: %s | Status: %s\n",
                     catalog[i].book_id, catalog[i].title, catalog[i].author, catalog[i].category,
                     catalog[i].is_available ? "AVAILABLE" : "ISSUED");
            strcat(response, temp);
        }
    }

    if (!found) strcat(response, "No matching books found.\n");
    strcat(response, "EOF");
    send(sock, response, strlen(response), 0);
    log_transaction("Search query processed.");
}

void handle_check_availability(int sock) {
    char response[BUFFER_SIZE] = "=== CATALOG AVAILABILITY ===\n";
    char temp[256];

    for (int i = 0; i < MAX_BOOKS; i++) {
        snprintf(temp, sizeof(temp), "ID: %d | Title: %s | Status: %s\n",
                 catalog[i].book_id, catalog[i].title, 
                 catalog[i].is_available ? "AVAILABLE" : "ISSUED");
        strcat(response, temp);
    }
    strcat(response, "EOF");
    send(sock, response, strlen(response), 0);
    log_transaction("Catalog availability checked.");
}

void handle_issue_book(int sock) {
    int sid = 0, bid = 0;
    char response[BUFFER_SIZE] = {0}, log_msg[256] = {0};

    recv(sock, &sid, sizeof(int), 0);
    recv(sock, &bid, sizeof(int), 0);

    int book_idx = -1;
    for (int i = 0; i < MAX_BOOKS; i++) {
        if (catalog[i].book_id == bid) { book_idx = i; break; }
    }

    if (book_idx == -1) {
        snprintf(response, sizeof(response), "[ERROR] Invalid Book ID (%d)!\n", bid);
        send(sock, response, strlen(response), 0);
        return;
    }

    if (!catalog[book_idx].is_available) {
        snprintf(response, sizeof(response), "[ERROR] Book '%s' is already issued to another student!\n", catalog[book_idx].title);
        send(sock, response, strlen(response), 0);
        return;
    }

    // Process Issue
    catalog[book_idx].is_available = 0;
    int issue_id = 7000 + record_count + 1;
    records[record_count] = (IssueRecord){issue_id, sid, bid, "ISSUED"};
    record_count++;

    snprintf(response, sizeof(response), 
             "[CONFIRMATION] Book Issued Successfully!\nTransaction ID: %d | Title: %s | Student ID: %d\n", 
             issue_id, catalog[book_idx].title, sid);
    send(sock, response, strlen(response), 0);

    snprintf(log_msg, sizeof(log_msg), "Issued Book ID %d ('%s') to Student %d (Trans ID: %d)", 
             bid, catalog[book_idx].title, sid, issue_id);
    log_transaction(log_msg);
}

void handle_return_book(int sock) {
    int issue_id = 0;
    char response[BUFFER_SIZE] = {0}, log_msg[256] = {0};

    recv(sock, &issue_id, sizeof(int), 0);

    int record_idx = -1;
    for (int i = 0; i < record_count; i++) {
        if (records[i].issue_id == issue_id && strcmp(records[i].status, "ISSUED") == 0) {
            record_idx = i;
            break;
        }
    }

    if (record_idx == -1) {
        snprintf(response, sizeof(response), "[ERROR] Active Issue Transaction ID (%d) not found!\n", issue_id);
        send(sock, response, strlen(response), 0);
        return;
    }

    // Process Return
    int bid = records[record_idx].book_id;
    for (int i = 0; i < MAX_BOOKS; i++) {
        if (catalog[i].book_id == bid) {
            catalog[i].is_available = 1; // Mark available again
            break;
        }
    }

    strcpy(records[record_idx].status, "RETURNED");

    snprintf(response, sizeof(response), "[SUCCESS] Book returned successfully for Transaction ID %d.\n", issue_id);
    send(sock, response, strlen(response), 0);

    snprintf(log_msg, sizeof(log_msg), "Returned Book for Transaction ID %d", issue_id);
    log_transaction(log_msg);
}

void handle_view_issued_books(int sock) {
    int sid = 0;
    char response[BUFFER_SIZE] = "=== ISSUED BOOKS HISTORY ===\n";
    char temp[256];

    recv(sock, &sid, sizeof(int), 0);

    int found = 0;
    for (int i = 0; i < record_count; i++) {
        if (records[i].student_id == sid) {
            found = 1;
            int book_idx = -1;
            for (int b = 0; b < MAX_BOOKS; b++) {
                if (catalog[b].book_id == records[i].book_id) { book_idx = b; break; }
            }

            snprintf(temp, sizeof(temp), "Trans ID: %d | Book: %s | Status: %s\n",
                     records[i].issue_id,
                     (book_idx != -1) ? catalog[book_idx].title : "Unknown",
                     records[i].status);
            strcat(response, temp);
        }
    }

    if (!found) strcat(response, "No issued books record found for this Student ID.\n");
    strcat(response, "EOF");
    send(sock, response, strlen(response), 0);
    log_transaction("Viewed student issued books.");
}

int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int option;

    init_catalog();

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("[+] Iterative TCP Library Server running on port %d...\n", PORT);

    // ITERATIVE SERVER LOOP
    while (1) {
        printf("\n---------------------------------------------\n");
        printf("[+] Waiting for a client connection...\n");

        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            continue;
        }

        printf("[+] Client connected! Servicing active client...\n");
        log_transaction("Client connected.");

        // Client Request Loop
        while (1) {
            if (recv(client_socket, &option, sizeof(int), 0) <= 0) break;

            if (option == 1) handle_search_books(client_socket);
            else if (option == 2) handle_check_availability(client_socket);
            else if (option == 3) handle_issue_book(client_socket);
            else if (option == 4) handle_return_book(client_socket);
            else if (option == 5) handle_view_issued_books(client_socket);
            else if (option == 6) {
                printf("[-] Client disconnected cleanly.\n");
                log_transaction("Client disconnected.");
                break;
            }
        }

        close(client_socket);
        printf("[+] Session finished. Ready for next client in line.\n");
    }

    close(server_fd);
    return 0;
}
