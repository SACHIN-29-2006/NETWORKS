// receiver.c - Go-Back-N Receiver Simulation
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define PACKET_SIZE 64

typedef struct {
    int seq_num;
    int simulate_ack_loss;
    char data[PACKET_SIZE];
} Frame;

typedef struct {
    int ack_num;
} AckFrame;

// --- SINGLY LINKED LIST EVENT LOG (mirrors sender's) ---
typedef struct LogNode {
    char message[160];
    struct LogNode* next;
} LogNode;

typedef struct {
    LogNode* head;
    LogNode* tail;
} EventLog;

void logInit(EventLog* l) { l->head = l->tail = NULL; }

void logAdd(EventLog* l, const char* fmt, ...) {
    LogNode* n = (LogNode*)malloc(sizeof(LogNode));
    va_list args;
    va_start(args, fmt);
    vsnprintf(n->message, sizeof(n->message), fmt, args);
    va_end(args);
    n->next = NULL;
    if (l->tail == NULL) l->head = l->tail = n;
    else { l->tail->next = n; l->tail = n; }
}

void logPrintAll(EventLog* l) {
    int i = 1;
    for (LogNode* c = l->head; c != NULL; c = c->next, i++) {
        printf("  %3d. %s\n", i, c->message);
    }
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) { perror("Socket failed"); exit(EXIT_FAILURE); }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) { perror("Bind failed"); exit(EXIT_FAILURE); }
    if (listen(server_fd, 3) < 0) { perror("Listen failed"); exit(EXIT_FAILURE); }

    printf("[RECEIVER] Waiting for Sender connection on port %d...\n", PORT);
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("Accept failed"); exit(EXIT_FAILURE);
    }
    printf("[RECEIVER] Sender connected successfully!\n");

    int window_size = 0;
    recv(new_socket, &window_size, sizeof(int), 0);
    int modulo = 1;
    while (modulo <= window_size) modulo *= 2;
    printf("[RECEIVER CONFIG] Window Size (N) = %d | Modulo = %d\n\n", window_size, modulo);

    EventLog log; logInit(&log);
    int accepted_count = 0, discarded_count = 0, withheld_count = 0;
    int expected_seq = 0; // classic Go-Back-N: only this exact next in-order frame is ever accepted

    while (1) {
        Frame rx_frame;
        int bytes_read = recv(new_socket, &rx_frame, sizeof(Frame), 0);
        if (bytes_read <= 0) {
            printf("\n[RECEIVER] Sender closed connection - all frames delivered. Exiting...\n");
            break;
        }

        printf("\n--------------------------------------------------\n");
        printf("[RECEIVER] Frame Arrived -> Seq: %d | Data: \"%s\"\n", rx_frame.seq_num, rx_frame.data);

        if (rx_frame.seq_num != expected_seq) {
            printf("--> [DISCARD] Expected Seq %d - ignoring out-of-order/duplicate frame, no ACK sent.\n", expected_seq);
            logAdd(&log, "Frame %d: Discarded (out-of-order/duplicate, expected %d)", rx_frame.seq_num, expected_seq);
            discarded_count++;
            continue;
        }

        printf("--> [SUCCESS] Frame %d verified in-order!\n", rx_frame.seq_num);

        if (rx_frame.simulate_ack_loss == 1) {
            printf("--> [SIMULATION] Withholding ACK for Seq %d (simulated loss).\n", rx_frame.seq_num);
            logAdd(&log, "Frame %d: Received but ACK withheld (simulated loss)", rx_frame.seq_num);
            withheld_count++;
            continue; // expected_seq unchanged - we're still waiting on this same frame
        }

        printf("--> [AUTO-ACK] Sending ACK %d.\n", expected_seq);
        logAdd(&log, "Frame %d: Accepted -> ACK %d sent", rx_frame.seq_num, expected_seq);
        accepted_count++;

        AckFrame ack;
        ack.ack_num = expected_seq;
        send(new_socket, &ack, sizeof(AckFrame), 0);

        expected_seq = (expected_seq + 1) % modulo;
    }

    printf("\n================ RECEIVER LOG ================\n");
    logPrintAll(&log);

    printf("\n================ RECEIVER SUMMARY ================\n");
    printf("Frames accepted            : %d\n", accepted_count);
    printf("Frames discarded (dup/oos) : %d\n", discarded_count);
    printf("ACKs withheld (simulated)  : %d\n", withheld_count);

    close(new_socket);
    close(server_fd);
    return 0;
}
