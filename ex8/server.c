// sender.c - Go-Back-N Sender Simulation
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>

#define PORT 8080
#define PACKET_SIZE 64
#define TIMEOUT_SEC 4

typedef struct {
    int seq_num;
    int simulate_ack_loss;
    char data[PACKET_SIZE];
} Frame;

typedef struct {
    int ack_num;
} AckFrame;

// --- SINGLY LINKED LIST WINDOW QUEUE ---

typedef struct Node {
    Frame frame;
    int creation_index; // unique 0..F-1 identity, unambiguous even after seq_num wraps
    int attempts;        // how many times this frame has been physically sent
    struct Node* next;
} Node;

typedef struct {
    Node* head;
    Node* tail;
    int count;
} Queue;

void initQueue(Queue* q) { q->head = q->tail = NULL; q->count = 0; }

Node* enqueue(Queue* q, Frame f, int creation_index) {
    Node* n = (Node*)malloc(sizeof(Node));
    n->frame = f;
    n->creation_index = creation_index;
    n->attempts = 0;
    n->next = NULL;
    if (q->tail == NULL) q->head = q->tail = n;
    else { q->tail->next = n; q->tail = n; }
    q->count++;
    return n;
}

void dequeue(Queue* q) {
    if (q->head == NULL) return;
    Node* t = q->head;
    q->head = q->head->next;
    if (q->head == NULL) q->tail = NULL;
    free(t);
    q->count--;
}

// --- SINGLY LINKED LIST EVENT LOG ---

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

// Transmit (or retransmit) a single frame. Applies the user-chosen

void transmitFrame(int sock, Node* node, int modulo, int impair_index, int impair_type,
                    int* total_tx, EventLog* log) {
    node->attempts++;
    Frame wire = node->frame; // wire copy - stored node stays clean for future retransmits
    int first_attempt_on_target = (node->attempts == 1 && node->creation_index == impair_index);

    if (first_attempt_on_target && impair_type == 1) {
        wire.simulate_ack_loss = 1;
        logAdd(log, "Frame %d: Transmitted (Attempt 1) -- SIMULATED LOSS", wire.seq_num);
    } else if (first_attempt_on_target && impair_type == 2) {
        int corrupted = (wire.seq_num + 1) % modulo;
        logAdd(log, "Frame %d: Transmitted (Attempt 1) -- SIMULATED CORRUPTION (sent on wire as Seq %d)",
               node->frame.seq_num, corrupted);
        wire.seq_num = corrupted;
    } else if (node->attempts == 1) {
        logAdd(log, "Frame %d: Transmitted (Attempt 1)", wire.seq_num);
    } else {
        logAdd(log, "Frame %d: Retransmitted (Attempt %d) [Go-Back-N]", wire.seq_num, node->attempts);
    }

    printf("--> Sending Frame [Seq: %d, Data: \"%s\"] (Attempt %d)\n", wire.seq_num, wire.data, node->attempts);
    send(sock, &wire, sizeof(Frame), 0);
    (*total_tx)++;
}

void retransmitAll(int sock, Queue* q, int modulo, int impair_index, int impair_type,
                    int* total_tx, EventLog* log) {
    printf("\n--> [GO-BACK-N] Retransmitting entire window (%d frames)...\n", q->count);
    for (Node* c = q->head; c != NULL; c = c->next)
        transmitFrame(sock, c, modulo, impair_index, impair_type, total_tx, log);
}

void fillWindow(int sock, Queue* q, int window_size, int total_frames, int modulo,
                 int* next_to_send, int impair_index, int impair_type,
                 int* total_tx, EventLog* log) {
    while (q->count < window_size && *next_to_send < total_frames) {
        Frame f;
        f.seq_num = (*next_to_send) % modulo;
        f.simulate_ack_loss = 0;
        snprintf(f.data, PACKET_SIZE, "Data-%d", *next_to_send);

        Node* n = enqueue(q, f, *next_to_send);
        transmitFrame(sock, n, modulo, impair_index, impair_type, total_tx, log);
        (*next_to_send)++;
    }
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { perror("Socket creation error"); return -1; }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) { perror("Invalid address"); return -1; }

    struct timeval tv = { TIMEOUT_SEC, 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    printf("[SENDER] Connecting to Receiver on port %d...\n", PORT);
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) { perror("Connection Failed"); return -1; }
    printf("[SENDER] Connected successfully!\n\n");

    int window_size, total_frames, impair_index, impair_type = 0;

    printf("Enter Go-Back-N Window Size N: ");
    scanf("%d", &window_size);

    printf("Enter total number of frames to send: ");
    scanf("%d", &total_frames);

    printf("Enter the frame number to simulate as lost/corrupted (0 to %d, or -1 for none): ",
           total_frames - 1);
    scanf("%d", &impair_index);

    if (impair_index >= 0 && impair_index < total_frames) {
        printf("Choose impairment type for Frame %d:\n", impair_index);
        printf("  1. Lost (ACK swallowed by receiver, sender must timeout)\n");
        printf("  2. Corrupted (wrong sequence number on the wire)\n");
        printf("Choice (1-2): ");
        scanf("%d", &impair_type);
    } else {
        impair_index = -1; // out of range or -1 -> no impairment
    }

    send(sock, &window_size, sizeof(int), 0);

    int modulo = 1;
    while (modulo <= window_size) modulo *= 2;
    printf("\n[SENDER CONFIG] N = %d | Total Frames = %d | Modulo = %d", window_size, total_frames, modulo);
    if (impair_index >= 0)
        printf(" | Impairing Frame %d as %s\n", impair_index, impair_type == 1 ? "LOST" : "CORRUPTED");
    else
        printf(" | No impairment (ideal channel)\n");
    printf("\n");

    Queue txQueue; initQueue(&txQueue);
    EventLog log; logInit(&log);
    int total_tx = 0;
    int next_to_send = 0;

    printf("--- FILLING INITIAL WINDOW (up to %d frames) ---\n", window_size);
    fillWindow(sock, &txQueue, window_size, total_frames, modulo, &next_to_send,
               impair_index, impair_type, &total_tx, &log);

    // --- SLIDING WINDOW LOOP: run until every frame has been ACKed ---
    while (txQueue.count > 0) {
        int expected_ack = txQueue.head->frame.seq_num;
        printf("\nWaiting for ACK on Queue Head (Seq %d)...\n", expected_ack);

        AckFrame ack;
        int bytes_read = recv(sock, &ack, sizeof(AckFrame), 0);

        if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("\n--> [TIMEOUT] No ACK for Seq %d within %ds!\n", expected_ack, TIMEOUT_SEC);
                logAdd(&log, "Timeout waiting for ACK %d -> Go-Back-N retransmission triggered", expected_ack);
                retransmitAll(sock, &txQueue, modulo, impair_index, impair_type, &total_tx, &log);
            } else {
                perror("recv error");
                break;
            }
            continue;
        }
        if (bytes_read == 0) { printf("[SENDER] Receiver closed connection.\n"); break; }

        printf("--> Received ACK %d from Receiver.\n", ack.ack_num);

        if (ack.ack_num != expected_ack) {
            printf("--> [UNEXPECTED ACK] Expected %d, got %d.\n", expected_ack, ack.ack_num);
            logAdd(&log, "Unexpected ACK %d (expected %d) -> Go-Back-N retransmission triggered",
                   ack.ack_num, expected_ack);
            retransmitAll(sock, &txQueue, modulo, impair_index, impair_type, &total_tx, &log);
            continue;
        }

        printf("--> [ACK MATCH] Frame %d delivered. Sliding window.\n", expected_ack);
        logAdd(&log, "Frame %d: ACK received -> Delivered successfully after %d attempt(s)",
               expected_ack, txQueue.head->attempts);
        dequeue(&txQueue);

        fillWindow(sock, &txQueue, window_size, total_frames, modulo, &next_to_send,
                   impair_index, impair_type, &total_tx, &log);
    }

    printf("\n================ TRANSMISSION LOG ================\n");
    logPrintAll(&log);

    printf("\n================ ANALYSIS ================\n");
    printf("Total unique frames to deliver : %d\n", total_frames);
    printf("Total physical transmissions   : %d\n", total_tx);
    printf("Retransmissions (overhead)     : %d\n", total_tx - total_frames);
    printf("Transmission efficiency        : %.2f%%\n", 100.0 * total_frames / total_tx);

    close(sock);
    return 0;
}
