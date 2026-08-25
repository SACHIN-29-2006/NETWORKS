#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define TIMEOUT_SEC 2
#define MAX_RETRIES 3

#define TYPE_DATA 1
#define TYPE_QUERY 2

typedef struct __attribute__((packed)) {
    int pkt_type;
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

unsigned char calculate_checksum(WeatherPacket *pkt) {
    unsigned char *ptr = (unsigned char *)pkt;
    unsigned int sum = 0;
    size_t payload_len = sizeof(WeatherPacket) - sizeof(unsigned char);
    for (size_t i = 0; i < payload_len; i++) {
        sum += ptr[i];
    }
    return (unsigned char)(~sum & 0xFF);
}

void send_weather_update(int client_fd, struct sockaddr_in *server_addr, char *location, int *seq_num) {
    WeatherPacket pkt;
    memset(&pkt, 0, sizeof(WeatherPacket));

    pkt.pkt_type = TYPE_DATA;
    pkt.seq_num = *seq_num;
    strncpy(pkt.location, location, 31);

    printf("\n--- Input Weather Parameters for [%s] ---\n", location);
    printf("Enter Temperature (C): "); scanf("%f", &pkt.temperature);
    printf("Enter Humidity (%%): "); scanf("%f", &pkt.humidity);
    printf("Enter Pressure (hPa): "); scanf("%f", &pkt.pressure);
    printf("Enter Wind Speed (km/h): "); scanf("%f", &pkt.wind_speed);

    pkt.checksum = calculate_checksum(&pkt);

    socklen_t addr_len = sizeof(*server_addr);
    int ack_received = 0;
    int retries = 0;

    // Packet Retransmission Loop with Timeout Handling
    while (!ack_received && retries < MAX_RETRIES) {
        printf("[CLIENT] Transmitting Packet Seq %d (Attempt %d/%d)...\n", 
               *seq_num, retries + 1, MAX_RETRIES);
        
        sendto(client_fd, &pkt, sizeof(WeatherPacket), 0, (struct sockaddr *)server_addr, addr_len);

        char ack_buffer[64] = {0};
        int bytes = recvfrom(client_fd, ack_buffer, sizeof(ack_buffer) - 1, 0, NULL, NULL);

        if (bytes > 0) {
            ack_buffer[bytes] = '\0';
            printf("[CLIENT SUCCESS] Server Response: %s\n", ack_buffer);
            ack_received = 1;
            (*seq_num)++;
        } else {
            printf("[CLIENT TIMEOUT] Server ACK timed out. Initiating retransmission...\n");
            retries++;
        }
    }

    if (!ack_received) {
        printf("[CLIENT ERROR] Connection failed. Max retransmissions reached for Seq %d.\n", *seq_num);
    }
}

void query_statistics(int client_fd, struct sockaddr_in *server_addr) {
    WeatherPacket pkt;
    memset(&pkt, 0, sizeof(WeatherPacket));

    char target_location[32];
    printf("\nEnter location name to fetch statistics: ");
    scanf("%31s", target_location);

    pkt.pkt_type = TYPE_QUERY;
    strncpy(pkt.location, target_location, 31);
    pkt.checksum = calculate_checksum(&pkt);

    socklen_t addr_len = sizeof(*server_addr);

    printf("[CLIENT] Querying server for stats on [%s]...\n", target_location);
    sendto(client_fd, &pkt, sizeof(WeatherPacket), 0, (struct sockaddr *)server_addr, addr_len);

    StatsResponse resp;
    memset(&resp, 0, sizeof(StatsResponse));

    int bytes = recvfrom(client_fd, &resp, sizeof(StatsResponse), 0, NULL, NULL);

    if (bytes > 0) {
        printf("\n==================================================\n");
        printf("       WEATHER STATISTICS FOR [%s]              \n", resp.location);
        printf("==================================================\n");
        if (resp.total_packets == 0) {
            printf("No records found for this location!\n");
        } else {
            printf("Total Samples Collected : %d\n", resp.total_packets);
            printf("Average Temperature     : %.2f C\n", resp.avg_temp);
            printf("Average Humidity        : %.2f %%\n", resp.avg_hum);
            printf("Average Pressure        : %.2f hPa\n", resp.avg_press);
            printf("Average Wind Speed      : %.2f km/h\n", resp.avg_wind);
        }
        printf("==================================================\n");
    } else {
        printf("[CLIENT ERROR] Query request timed out. Server non-responsive.\n");
    }
}

int main(int argc, char *argv[]) {
    int client_fd;
    struct sockaddr_in server_addr;
    struct timeval tv;
    char location[32] = "Station-Alpha";

    if (argc > 1) strncpy(location, argv[1], 31);

    client_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (client_fd < 0) { 
        perror("Socket creation failed"); 
        exit(EXIT_FAILURE); 
    }

    // Configure Application-Level Socket Timeout (2 seconds)
    tv.tv_sec = TIMEOUT_SEC;
    tv.tv_usec = 0;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    int seq_num = 1;
    int choice;

    while (1) {
        printf("\n--- Weather Client Menu (%s) ---\n", location);
        printf("1. Send Weather Data Update\n");
        printf("2. Query Weather Statistics\n");
        printf("3. Exit\n");
        printf("Select Option: ");
        if (scanf("%d", &choice) != 1) break;

        if (choice == 1) {
            send_weather_update(client_fd, &server_addr, location, &seq_num);
        } else if (choice == 2) {
            query_statistics(client_fd, &server_addr);
        } else if (choice == 3) {
            break;
        } else {
            printf("Invalid selection.\n");
        }
    }

    close(client_fd);
    return 0;
}
