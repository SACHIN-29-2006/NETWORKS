#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define MAX_RECORDS 20

#define TYPE_DATA 1
#define TYPE_QUERY 2

// Packed struct ensures byte-for-byte memory alignment across compilers
typedef struct __attribute__((packed)) {
    int pkt_type;            // 1 = Sensor Data Update, 2 = Stats Query
    int seq_num;
    char location[32];
    float temperature;
    float humidity;
    float pressure;
    float wind_speed;
    unsigned char checksum;
} WeatherPacket;

typedef struct __attribute__((packed)) {
    char location[32];
    int total_packets;
    float avg_temp;
    float avg_hum;
    float avg_press;
    float avg_wind;
} StatsResponse;

// Track individual client sessions by IP + Port for sequence numbers
typedef struct {
    struct sockaddr_in addr;
    int expected_seq;
} ClientSession;

// Aggregate weather stats per location name
typedef struct {
    char location[32];
    int total_packets;
    float sum_temp;
    float sum_hum;
    float sum_press;
    float sum_wind;
} LocationStats;

ClientSession clients[MAX_RECORDS];
int client_count = 0;

LocationStats stats_db[MAX_RECORDS];
int stats_count = 0;

// Application-level error detection using byte-wise inverted checksum
unsigned char calculate_checksum(WeatherPacket *pkt) {
    unsigned char *ptr = (unsigned char *)pkt;
    unsigned int sum = 0;
    size_t payload_len = sizeof(WeatherPacket) - sizeof(unsigned char);
    for (size_t i = 0; i < payload_len; i++) {
        sum += ptr[i];
    }
    return (unsigned char)(~sum & 0xFF);
}

// Find or register unique client connection (IP + Port)
ClientSession* get_or_create_client(struct sockaddr_in *cli_addr) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i].addr.sin_addr.s_addr == cli_addr->sin_addr.s_addr &&
            clients[i].addr.sin_port == cli_addr->sin_port) {
            return &clients[i];
        }
    }
    if (client_count < MAX_RECORDS) {
        clients[client_count].addr = *cli_addr;
        clients[client_count].expected_seq = 1;
        return &clients[client_count++];
    }
    return NULL;
}

// Find or create statistics aggregator for a target location
LocationStats* get_or_create_stats(const char *loc) {
    for (int i = 0; i < stats_count; i++) {
        if (strcmp(stats_db[i].location, loc) == 0) return &stats_db[i];
    }
    if (stats_count < MAX_RECORDS) {
        strcpy(stats_db[stats_count].location, loc);
        stats_db[stats_count].total_packets = 0;
        stats_db[stats_count].sum_temp = 0.0f;
        stats_db[stats_count].sum_hum = 0.0f;
        stats_db[stats_count].sum_press = 0.0f;
        stats_db[stats_count].sum_wind = 0.0f;
        return &stats_db[stats_count++];
    }
    return NULL;
}

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

    printf("==================================================\n");
    printf(" Real-Time Weather Server Listening on Port %d \n", PORT);
    printf("==================================================\n\n");

    while (1) {
        WeatherPacket pkt;
        memset(&pkt, 0, sizeof(WeatherPacket));

        int bytes = recvfrom(server_fd, &pkt, sizeof(WeatherPacket), 0,
                             (struct sockaddr *)&client_addr, &addr_len);
        if (bytes < 0) continue;

        // Error Detection Check
        if (calculate_checksum(&pkt) != pkt.checksum) {
            printf("[SERVER WARNING] Corrupted packet dropped from [%s:%d]\n",
                   inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            continue;
        }

        ClientSession *session = get_or_create_client(&client_addr);
        LocationStats *loc_stat = get_or_create_stats(pkt.location);

        if (pkt.pkt_type == TYPE_DATA) {
            // Sequence Number Processing
            if (pkt.seq_num < session->expected_seq) {
                printf("[SERVER REJECT] Duplicate packet Seq %d from [%s:%d]. Re-acknowledging.\n",
                       pkt.seq_num, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            } else {
                if (pkt.seq_num > session->expected_seq) {
                    printf("[SERVER WARNING] Packet gap detected! Expected Seq %d, received Seq %d from [%s:%d].\n",
                           session->expected_seq, pkt.seq_num,
                           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                }

                // Log and aggregate data
                loc_stat->total_packets++;
                loc_stat->sum_temp += pkt.temperature;
                loc_stat->sum_hum += pkt.humidity;
                loc_stat->sum_press += pkt.pressure;
                loc_stat->sum_wind += pkt.wind_speed;
                session->expected_seq = pkt.seq_num + 1;

                printf("[SERVER LOG] Packet Received from Location [%s] via Client [%s:%d] | Seq: %d\n",
                       pkt.location, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), pkt.seq_num);
                printf("             Temp: %.1fC | Hum: %.1f%% | Press: %.1fhPa | Wind: %.1fkm/h\n",
                       pkt.temperature, pkt.humidity, pkt.pressure, pkt.wind_speed);
            }

            // Respond with ACK Frame
            char ack[64];
            snprintf(ack, sizeof(ack), "ACK %d", pkt.seq_num);
            sendto(server_fd, ack, strlen(ack), 0, (struct sockaddr *)&client_addr, addr_len);

        } else if (pkt.pkt_type == TYPE_QUERY) {
            printf("[SERVER QUERY] Statistics Request from [%s:%d] for Location [%s]\n",
                   inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), pkt.location);

            StatsResponse resp;
            memset(&resp, 0, sizeof(StatsResponse));
            strncpy(resp.location, pkt.location, 31);

            if (loc_stat && loc_stat->total_packets > 0) {
                resp.total_packets = loc_stat->total_packets;
                resp.avg_temp = loc_stat->sum_temp / loc_stat->total_packets;
                resp.avg_hum = loc_stat->sum_hum / loc_stat->total_packets;
                resp.avg_press = loc_stat->sum_press / loc_stat->total_packets;
                resp.avg_wind = loc_stat->sum_wind / loc_stat->total_packets;
            } else {
                resp.total_packets = 0;
            }

            sendto(server_fd, &resp, sizeof(StatsResponse), 0, (struct sockaddr *)&client_addr, addr_len);
        }
    }

    close(server_fd);
    return 0;
}
