#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#define PORT 8080
#define BUFFER_SIZE 2048
#define MAX_ROOMS 5
#define MAX_RESERVATIONS 100

typedef struct {
    int room_number;
    char type[20]; // "Single", "Double", "Suite"
    int price;
    int is_booked; // 0 = Available, 1 = Booked
} Room;

typedef struct {
    int res_id;
    int client_id;
    int room_number;
    char status[20]; // "BOOKED", "CANCELLED"
} Reservation;

// Request Payload Structure
typedef struct {
    int action; // 1: View, 2: Book, 3: Cancel, 4: Check
    int client_id;
    int room_number;
    int res_id;
} UDPRequest;

// Argument structure passed to worker threads
typedef struct {
    int server_sock;
    struct sockaddr_in client_addr;
    socklen_t addr_len;
    UDPRequest req;
} ThreadArgs;

// Shared Data Resources
Room rooms[MAX_ROOMS];
Reservation reservations[MAX_RESERVATIONS];
int res_count = 0;

// Synchronization Primitives
pthread_mutex_t db_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

// Logging Utility
void log_transaction(const char *msg) {
    pthread_mutex_lock(&log_mutex);
    FILE *log_file = fopen("hotel_log.txt", "a");
    if (log_file) {
        time_t now = time(NULL);
        char *timestamp = ctime(&now);
        timestamp[strcspn(timestamp, "\n")] = 0;
        fprintf(log_file, "[%s] %s\n", timestamp, msg);
        fclose(log_file);
    }
    pthread_mutex_unlock(&log_mutex);
}

// Save active database to file
void save_database_to_file() {
    FILE *db = fopen("hotel_db.txt", "w");
    if (!db) return;

    fprintf(db, "--- ROOM STATUS ---\n");
    for (int i = 0; i < MAX_ROOMS; i++) {
        fprintf(db, "Room %d | Type: %s | Price: $%d | Status: %s\n",
                rooms[i].room_number, rooms[i].type, rooms[i].price,
                rooms[i].is_booked ? "BOOKED" : "AVAILABLE");
    }

    fprintf(db, "\n--- RESERVATIONS ---\n");
    for (int i = 0; i < res_count; i++) {
        fprintf(db, "ResID: %d | ClientID: %d | Room: %d | Status: %s\n",
                reservations[i].res_id, reservations[i].client_id,
                reservations[i].room_number, reservations[i].status);
    }
    fclose(db);
}

void init_hotel() {
    rooms[0] = (Room){101, "Single", 100, 0};
    rooms[1] = (Room){102, "Single", 100, 0};
    rooms[2] = (Room){201, "Double", 180, 0};
    rooms[3] = (Room){202, "Double", 180, 0};
    rooms[4] = (Room){301, "Suite",  350, 0};

    save_database_to_file();
    log_transaction("Hotel Reservation Database Initialized.");
}

// Thread Handler for Worker Tasks
void* handle_client_request(void* args) {
    ThreadArgs* t_args = (ThreadArgs*)args;
    char response[BUFFER_SIZE] = {0};
    char temp[256];
    char log_msg[256];

    int action = t_args->req.action;

    if (action == 1) { // View Availability
        pthread_mutex_lock(&db_mutex);
        strcpy(response, "=== ROOM AVAILABILITY ===\n");
        for (int i = 0; i < MAX_ROOMS; i++) {
            snprintf(temp, sizeof(temp), "Room: %d | Type: %-6s | Price: $%d | Status: %s\n",
                     rooms[i].room_number, rooms[i].type, rooms[i].price,
                     rooms[i].is_booked ? "BOOKED" : "AVAILABLE");
            strcat(response, temp);
        }
        pthread_mutex_unlock(&db_mutex);

        snprintf(log_msg, sizeof(log_msg), "Client %d checked room availability.", t_args->req.client_id);
        log_transaction(log_msg);

    } else if (action == 2) { // Book Room (Mutex Synchronized)
        int rnum = t_args->req.room_number;
        int cid = t_args->req.client_id;

        pthread_mutex_lock(&db_mutex); // CRITICAL SECTION START

        int room_idx = -1;
        for (int i = 0; i < MAX_ROOMS; i++) {
            if (rooms[i].room_number == rnum) { room_idx = i; break; }
        }

        if (room_idx == -1) {
            snprintf(response, sizeof(response), "[ERROR] Room %d does not exist!\n", rnum);
        } else if (rooms[room_idx].is_booked) {
            snprintf(response, sizeof(response), "[REJECTED] Room %d is already reserved by another client!\n", rnum);
        } else {
            // Book Room safely
            rooms[room_idx].is_booked = 1;
            int resid = 9000 + res_count + 1;
            
            reservations[res_count] = (Reservation){resid, cid, rnum, "BOOKED"};
            res_count++;

            save_database_to_file();

            snprintf(response, sizeof(response), 
                     "[SUCCESS] Reservation Confirmed!\nRes ID: %d | Room: %d | Client ID: %d\n", 
                     resid, rnum, cid);

            snprintf(log_msg, sizeof(log_msg), "SUCCESS: Client %d reserved Room %d (Res ID: %d)", cid, rnum, resid);
            log_transaction(log_msg);
        }

        pthread_mutex_unlock(&db_mutex); // CRITICAL SECTION END

    } else if (action == 3) { // Cancel Reservation
        int resid = t_args->req.res_id;

        pthread_mutex_lock(&db_mutex); // CRITICAL SECTION START

        int res_idx = -1;
        for (int i = 0; i < res_count; i++) {
            if (reservations[i].res_id == resid && strcmp(reservations[i].status, "BOOKED") == 0) {
                res_idx = i;
                break;
            }
        }

        if (res_idx == -1) {
            snprintf(response, sizeof(response), "[ERROR] Active Reservation ID %d not found!\n", resid);
        } else {
            int rnum = reservations[res_idx].room_number;
            for (int i = 0; i < MAX_ROOMS; i++) {
                if (rooms[i].room_number == rnum) {
                    rooms[i].is_booked = 0; // Free room
                    break;
                }
            }
            strcpy(reservations[res_idx].status, "CANCELLED");
            save_database_to_file();

            snprintf(response, sizeof(response), "[SUCCESS] Reservation ID %d cancelled. Room %d is now AVAILABLE.\n", resid, rnum);
            snprintf(log_msg, sizeof(log_msg), "CANCEL: Client cancelled Res ID %d (Room %d)", resid, rnum);
            log_transaction(log_msg);
        }

        pthread_mutex_unlock(&db_mutex); // CRITICAL SECTION END

    } else if (action == 4) { // Check Details
        int cid = t_args->req.client_id;

        pthread_mutex_lock(&db_mutex);
        strcpy(response, "=== BOOKING DETAILS ===\n");
        int found = 0;
        for (int i = 0; i < res_count; i++) {
            if (reservations[i].client_id == cid) {
                found = 1;
                snprintf(temp, sizeof(temp), "Res ID: %d | Room: %d | Status: %s\n",
                         reservations[i].res_id, reservations[i].room_number, reservations[i].status);
                strcat(response, temp);
            }
        }
        if (!found) strcat(response, "No booking records found for your Client ID.\n");
        pthread_mutex_unlock(&db_mutex);

        snprintf(log_msg, sizeof(log_msg), "Client %d requested booking details.", cid);
        log_transaction(log_msg);
    }

    // Send UDP Response Datagram back to client
    sendto(t_args->server_sock, response, strlen(response), 0,
           (struct sockaddr*)&t_args->client_addr, t_args->addr_len);

    free(t_args);
    pthread_exit(NULL);
}

int main() {
    int server_sock;
    struct sockaddr_in server_addr;

    init_hotel();

    if ((server_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("UDP Socket creation failed");
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

    printf("[+] Concurrent UDP Hotel Reservation Server active on port %d...\n", PORT);

    while (1) {
        ThreadArgs* t_args = (ThreadArgs*)malloc(sizeof(ThreadArgs));
        t_args->server_sock = server_sock;
        t_args->addr_len = sizeof(t_args->client_addr);

        // Receive UDP request datagram
        int bytes = recvfrom(server_sock, &t_args->req, sizeof(UDPRequest), 0,
                             (struct sockaddr*)&t_args->client_addr, &t_args->addr_len);

        if (bytes > 0) {
            pthread_t thread_id;
            // Spawn concurrent thread for incoming UDP packet
            if (pthread_create(&thread_id, NULL, handle_client_request, (void*)t_args) != 0) {
                perror("Thread creation failed");
                free(t_args);
            } else {
                pthread_detach(thread_id); // Auto cleanup on thread exit
            }
        } else {
            free(t_args);
        }
    }

    close(server_sock);
    return 0;
}
