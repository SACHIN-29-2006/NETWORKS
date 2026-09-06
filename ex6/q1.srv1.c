#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define PORT 8080
#define BUFFER_SIZE 2048
#define MAX_DOCTORS 3
#define MAX_SLOTS 4
#define MAX_PATIENTS 100
#define MAX_APPOINTMENTS 100

typedef struct {
    int id;
    char name[50];
    char specialty[50];
    char slots[MAX_SLOTS][20];
    int slot_available[MAX_SLOTS]; // 1 = Available, 0 = Booked
} Doctor;

typedef struct {
    int patient_id;
    char name[50];
    char phone[15];
} Patient;

typedef struct {
    int appt_id;
    int patient_id;
    int doctor_id;
    int slot_index;
    char status[20]; // "BOOKED", "CANCELLED"
} Appointment;

// Global System Data
Doctor doctors[MAX_DOCTORS];
Patient patients[MAX_PATIENTS];
Appointment appointments[MAX_APPOINTMENTS];

int patient_count = 0;
int appointment_count = 0;

// Logging Utility
void log_operation(const char *message) {
    FILE *log_file = fopen("hospital_log.txt", "a");
    if (log_file != NULL) {
        time_t now = time(NULL);
        char *timestamp = ctime(&now);
        timestamp[strcspn(timestamp, "\n")] = 0; // Strip newline
        fprintf(log_file, "[%s] %s\n", timestamp, message);
        fclose(log_file);
    }
}

// Data Initialization
void init_hospital_data() {
    doctors[0] = (Doctor){101, "Dr. Smith", "Cardiology", {"09:00 AM", "10:00 AM", "11:00 AM", "02:00 PM"}, {1, 1, 1, 1}};
    doctors[1] = (Doctor){102, "Dr. Adams", "Dermatology", {"10:00 AM", "11:30 AM", "01:00 PM", "03:30 PM"}, {1, 1, 1, 1}};
    doctors[2] = (Doctor){103, "Dr. Taylor", "Pediatrics", {"08:30 AM", "09:30 AM", "11:00 AM", "04:00 PM"}, {1, 1, 1, 1}};

    log_operation("Hospital System Data Initialized.");
}

// Command Handlers
void handle_register(int sock) {
    char name[50] = {0}, phone[15] = {0}, response[BUFFER_SIZE] = {0}, log_msg[256] = {0};
    
    recv(sock, name, sizeof(name) - 1, 0);
    recv(sock, phone, sizeof(phone) - 1, 0);

    name[strcspn(name, "\r\n")] = 0;
    phone[strcspn(phone, "\r\n")] = 0;

    int new_id = 1000 + patient_count + 1;
    patients[patient_count].patient_id = new_id;
    strncpy(patients[patient_count].name, name, 49);
    strncpy(patients[patient_count].phone, phone, 14);
    patient_count++;

    snprintf(response, sizeof(response), "[SUCCESS] Registration Complete! Your Patient ID is: %d\n", new_id);
    send(sock, response, strlen(response), 0);

    snprintf(log_msg, sizeof(log_msg), "Registered new patient: %s (ID: %d)", name, new_id);
    log_operation(log_msg);
}

void handle_search_doctors(int sock) {
    char response[BUFFER_SIZE] = "=== AVAILABLE DOCTORS & SLOTS ===\n";
    char temp[256];

    for (int i = 0; i < MAX_DOCTORS; i++) {
        snprintf(temp, sizeof(temp), "\nID: %d | Name: %s | Spec: %s\n  Available Slots:\n", 
                 doctors[i].id, doctors[i].name, doctors[i].specialty);
        strcat(response, temp);

        for (int j = 0; j < MAX_SLOTS; j++) {
            if (doctors[i].slot_available[j]) {
                snprintf(temp, sizeof(temp), "   [%d] %s\n", j + 1, doctors[i].slots[j]);
                strcat(response, temp);
            }
        }
    }
    strcat(response, "\nEOF");
    send(sock, response, strlen(response), 0);
    log_operation("Doctor availability searched.");
}

void handle_book_appointment(int sock) {
    int pid = 0, doc_id = 0, slot_num = 0;
    char response[BUFFER_SIZE] = {0}, log_msg[256] = {0};

    recv(sock, &pid, sizeof(int), 0);
    recv(sock, &doc_id, sizeof(int), 0);
    recv(sock, &slot_num, sizeof(int), 0);

    // Validate Patient ID
    int valid_patient = 0;
    for (int i = 0; i < patient_count; i++) {
        if (patients[i].patient_id == pid) { valid_patient = 1; break; }
    }

    if (!valid_patient) {
        snprintf(response, sizeof(response), "[ERROR] Invalid Patient ID (%d)! Please register first.\n", pid);
        send(sock, response, strlen(response), 0);
        return;
    }

    // Validate Doctor ID & Slot Index
    int doc_idx = -1;
    for (int i = 0; i < MAX_DOCTORS; i++) {
        if (doctors[i].id == doc_id) { doc_idx = i; break; }
    }

    if (doc_idx == -1 || slot_num < 1 || slot_num > MAX_SLOTS) {
        snprintf(response, sizeof(response), "[ERROR] Invalid Doctor ID or Slot Number!\n");
        send(sock, response, strlen(response), 0);
        return;
    }

    int slot_idx = slot_num - 1;
    if (!doctors[doc_idx].slot_available[slot_idx]) {
        snprintf(response, sizeof(response), "[ERROR] Slot already booked by another patient!\n");
        send(sock, response, strlen(response), 0);
        return;
    }

    // Process Booking
    doctors[doc_idx].slot_available[slot_idx] = 0; // Mark slot booked
    int appt_id = 5000 + appointment_count + 1;
    
    appointments[appointment_count] = (Appointment){appt_id, pid, doc_id, slot_idx, "BOOKED"};
    appointment_count++;

    snprintf(response, sizeof(response), 
             "[CONFIRMATION] Appointment Booked Successfully!\nAppt ID: %d | Doctor: %s | Time: %s\n", 
             appt_id, doctors[doc_idx].name, doctors[doc_idx].slots[slot_idx]);
    send(sock, response, strlen(response), 0);

    snprintf(log_msg, sizeof(log_msg), "Booked Appt ID %d for Patient %d with Doctor %d (Slot %s)", 
             appt_id, pid, doc_id, doctors[doc_idx].slots[slot_idx]);
    log_operation(log_msg);
}

void handle_cancel_appointment(int sock) {
    int appt_id = 0;
    char response[BUFFER_SIZE] = {0}, log_msg[256] = {0};

    recv(sock, &appt_id, sizeof(int), 0);

    int appt_idx = -1;
    for (int i = 0; i < appointment_count; i++) {
        if (appointments[i].appt_id == appt_id && strcmp(appointments[i].status, "BOOKED") == 0) {
            appt_idx = i;
            break;
        }
    }

    if (appt_idx == -1) {
        snprintf(response, sizeof(response), "[ERROR] Active Appointment ID (%d) not found!\n", appt_id);
        send(sock, response, strlen(response), 0);
        return;
    }

    // Restore slot and cancel
    int doc_id = appointments[appt_idx].doctor_id;
    int slot_idx = appointments[appt_idx].slot_index;

    for (int i = 0; i < MAX_DOCTORS; i++) {
        if (doctors[i].id == doc_id) {
            doctors[i].slot_available[slot_idx] = 1; // Mark slot available again
            break;
        }
    }

    strcpy(appointments[appt_idx].status, "CANCELLED");

    snprintf(response, sizeof(response), "[SUCCESS] Appointment %d cancelled successfully.\n", appt_id);
    send(sock, response, strlen(response), 0);

    snprintf(log_msg, sizeof(log_msg), "Cancelled Appt ID %d", appt_id);
    log_operation(log_msg);
}

void handle_view_history(int sock) {
    int pid = 0;
    char response[BUFFER_SIZE] = "=== APPOINTMENT HISTORY ===\n";
    char temp[256];

    recv(sock, &pid, sizeof(int), 0);

    int found = 0;
    for (int i = 0; i < appointment_count; i++) {
        if (appointments[i].patient_id == pid) {
            found = 1;
            int doc_idx = -1;
            for (int d = 0; d < MAX_DOCTORS; d++) {
                if (doctors[d].id == appointments[i].doctor_id) { doc_idx = d; break; }
            }

            snprintf(temp, sizeof(temp), "Appt ID: %d | Doctor: %s | Time: %s | Status: %s\n",
                     appointments[i].appt_id, 
                     (doc_idx != -1) ? doctors[doc_idx].name : "Unknown",
                     (doc_idx != -1) ? doctors[doc_idx].slots[appointments[i].slot_index] : "N/A",
                     appointments[i].status);
            strcat(response, temp);
        }
    }

    if (!found) {
        strcat(response, "No appointment history found for this Patient ID.\n");
    }

    strcat(response, "EOF");
    send(sock, response, strlen(response), 0);
    log_operation("Viewed appointment history.");
}

int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int option;

    init_hospital_data();

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

    printf("[+] Iterative TCP Hospital Server running on port %d...\n", PORT);

    // ITERATIVE SERVER LOOP
    while (1) {
        printf("\n---------------------------------------------\n");
        printf("[+] Waiting for a client connection...\n");

        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            continue;
        }

        printf("[+] Client connected! Servicing active client...\n");
        log_operation("Client connected.");

        // Session Command Loop
        while (1) {
            if (recv(client_socket, &option, sizeof(int), 0) <= 0) break;

            if (option == 1) handle_register(client_socket);
            else if (option == 2) handle_search_doctors(client_socket);
            else if (option == 3) handle_book_appointment(client_socket);
            else if (option == 4) handle_cancel_appointment(client_socket);
            else if (option == 5) handle_view_history(client_socket);
            else if (option == 6) {
                printf("[-] Client disconnected cleanly.\n");
                log_operation("Client disconnected.");
                break;
            }
        }

        close(client_socket);
        printf("[+] Session finished. Ready for next client in line.\n");
    }

    close(server_fd);
    return 0;
}
