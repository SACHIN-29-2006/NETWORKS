#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHANNEL_FILE "hamming_channel.txt"

#define BUFFER_SIZE 1000
#define MESSAGE_SIZE 2000

/* 96 MAC + 64 IP + 32 ports + 8 data = 200 bits */
#define LINK_FRAME_BITS 200

/* ---------------- Binary Utilities ---------------- */

void decimalToBinary(unsigned int value, int bits, char binary[]) {
    int i;

    binary[bits] = '\0';

    for (i = bits - 1; i >= 0; i--) {
        binary[i] = (value % 2) + '0';
        value = value / 2;
    }
}

int ipToBinary(const char *ip, char binary[]) {
    unsigned int a, b, c, d;
    char temp[9];

    if (sscanf(ip, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
        return 0;
    }

    if (a > 255 || b > 255 || c > 255 || d > 255) {
        return 0;
    }

    binary[0] = '\0';

    decimalToBinary(a, 8, temp);
    strcat(binary, temp);

    decimalToBinary(b, 8, temp);
    strcat(binary, temp);

    decimalToBinary(c, 8, temp);
    strcat(binary, temp);

    decimalToBinary(d, 8, temp);
    strcat(binary, temp);

    return 1;
}

int macToBinary(const char *mac, char binary[]) {
    unsigned int b1, b2, b3, b4, b5, b6;
    char temp[9];

    if (sscanf(mac, "%x:%x:%x:%x:%x:%x",
               &b1, &b2, &b3, &b4, &b5, &b6) != 6) {
        return 0;
    }

    if (b1 > 255 || b2 > 255 || b3 > 255 ||
        b4 > 255 || b5 > 255 || b6 > 255) {
        return 0;
    }

    binary[0] = '\0';

    decimalToBinary(b1, 8, temp);
    strcat(binary, temp);

    decimalToBinary(b2, 8, temp);
    strcat(binary, temp);

    decimalToBinary(b3, 8, temp);
    strcat(binary, temp);

    decimalToBinary(b4, 8, temp);
    strcat(binary, temp);

    decimalToBinary(b5, 8, temp);
    strcat(binary, temp);

    decimalToBinary(b6, 8, temp);
    strcat(binary, temp);

    return 1;
}

/* ---------------- Hamming Encoder ---------------- */

/*
   Positions 1, 2, 4, 8, 16, ... hold parity bits.
   All other positions hold data bits.
*/
int isPowerOfTwo(int position) {
    if (position < 1) {
        return 0;
    }

    if ((position & (position - 1)) == 0) {
        return 1;
    }

    return 0;
}

/*
   Chooses the smallest r such that:

       2^r  >=  dataBits + r + 1
*/
int countParityBits(int dataBits) {
    int r;

    r = 0;

    while ((1 << r) < dataBits + r + 1) {
        r = r + 1;
    }

    return r;
}

/*
   Syndrome is the XOR of every position that holds a '1'.
   A correct codeword always produces a syndrome of zero.
*/
int calculateSyndrome(const char codeword[]) {
    int length;
    int syndrome;
    int i;

    length = strlen(codeword);
    syndrome = 0;

    for (i = 1; i <= length; i++) {
        if (codeword[i - 1] == '1') {
            syndrome = syndrome ^ i;
        }
    }

    return syndrome;
}

/*
   Builds the Hamming codeword.

   Step 1: place data bits into non-parity positions
   Step 2: compute the syndrome of that partial codeword
   Step 3: copy the syndrome bits into the parity positions
*/
int hammingEncode(const char data[], char codeword[]) {
    int dataBits;
    int parityBits;
    int totalBits;
    int syndrome;
    int i;
    int j;
    int k;

    dataBits = strlen(data);
    parityBits = countParityBits(dataBits);
    totalBits = dataBits + parityBits;

    for (i = 0; i < totalBits; i++) {
        codeword[i] = '0';
    }

    codeword[totalBits] = '\0';

    j = 0;

    for (i = 1; i <= totalBits; i++) {
        if (!isPowerOfTwo(i)) {
            codeword[i - 1] = data[j];
            j = j + 1;
        }
    }

    syndrome = calculateSyndrome(codeword);

    for (k = 0; (1 << k) <= totalBits; k++) {
        if (syndrome & (1 << k)) {
            codeword[(1 << k) - 1] = '1';
        }
    }

    return totalBits;
}

void printParityPositions(const char codeword[]) {
    int totalBits;
    int k;
    int position;

    totalBits = strlen(codeword);

    printf(" [HAMMING]     Parity bits:");

    for (k = 0; (1 << k) <= totalBits; k++) {
        position = 1 << k;

        printf(" P%d=%c", position, codeword[position - 1]);
    }

    printf("\n");
}

/* ---------------- Sender OSI Layers ---------------- */

void applicationLayer(unsigned char character, char pdu[]) {
    decimalToBinary(character, 8, pdu);

    printf(" [APP LAYER]   Character '%c' -> %s\n",
           character, pdu);
}

void transportLayer(
    char pdu[],
    unsigned int sourcePort,
    unsigned int destinationPort
) {
    char sourcePortBinary[17];
    char destinationPortBinary[17];
    char temporary[BUFFER_SIZE];

    decimalToBinary(sourcePort, 16, sourcePortBinary);
    decimalToBinary(destinationPort, 16, destinationPortBinary);

    /* Source port + destination port + application data */
    sprintf(temporary, "%s%s%s",
            sourcePortBinary,
            destinationPortBinary,
            pdu);

    strcpy(pdu, temporary);

    printf(" [TRANS LAYER] Added ports: %d bits\n",
           (int)strlen(pdu));
}

int networkLayer(
    char pdu[],
    const char *sourceIP,
    const char *destinationIP
) {
    char sourceIPBinary[33];
    char destinationIPBinary[33];
    char temporary[BUFFER_SIZE];

    if (!ipToBinary(sourceIP, sourceIPBinary) ||
        !ipToBinary(destinationIP, destinationIPBinary)) {
        printf(" [NET LAYER]   Invalid IP address.\n");
        return 0;
    }

    /* Destination IP + source IP + transport segment */
    sprintf(temporary, "%s%s%s",
            destinationIPBinary,
            sourceIPBinary,
            pdu);

    strcpy(pdu, temporary);

    printf(" [NET LAYER]   Added IPs: %d bits\n",
           (int)strlen(pdu));

    return 1;
}

int dataLinkLayer(
    char pdu[],
    const char *sourceMAC,
    const char *destinationMAC
) {
    char sourceMACBinary[49];
    char destinationMACBinary[49];
    char linkFrame[BUFFER_SIZE];
    char codeword[BUFFER_SIZE];
    int totalBits;

    if (!macToBinary(sourceMAC, sourceMACBinary) ||
        !macToBinary(destinationMAC, destinationMACBinary)) {
        printf(" [LINK LAYER]  Invalid MAC address.\n");
        return 0;
    }

    /* Destination MAC + source MAC + network PDU */
    sprintf(linkFrame, "%s%s%s",
            destinationMACBinary,
            sourceMACBinary,
            pdu);

    if ((int)strlen(linkFrame) != LINK_FRAME_BITS) {
        printf(" [LINK LAYER]  Expected %d bits, got %d.\n",
               LINK_FRAME_BITS,
               (int)strlen(linkFrame));
        return 0;
    }

    printf(" [LINK LAYER]  Added MACs: %d bits\n",
           (int)strlen(linkFrame));

    /* Apply Hamming single error correction encoding */
    totalBits = hammingEncode(linkFrame, codeword);

    printf(" [HAMMING]     Data bits:   %d\n", LINK_FRAME_BITS);
    printf(" [HAMMING]     Parity bits: %d\n",
           totalBits - LINK_FRAME_BITS);
    printf(" [HAMMING]     Codeword:    %d bits\n", totalBits);

    printParityPositions(codeword);

    printf(" [HAMMING]     Verification syndrome: %d\n",
           calculateSyndrome(codeword));

    strcpy(pdu, codeword);

    printf(" [LINK LAYER]  Frame: %s\n", pdu);

    return 1;
}

/* ---------------- Main ---------------- */

int main(void) {
    char message[MESSAGE_SIZE];
    char pdu[BUFFER_SIZE];

    unsigned int sourcePort;
    unsigned int destinationPort;

    char sourceIP[] = "192.168.1.10";
    char destinationIP[] = "142.250.190.78";

    char sourceMAC[] = "AA:BB:CC:11:22:33";
    char destinationMAC[] = "DD:EE:FF:44:55:66";

    FILE *channel;
    int i;

    sourcePort = 54321;
    destinationPort = 8080;

    printf("Message to send: ");

    if (fgets(message, MESSAGE_SIZE, stdin) == NULL) {
        printf("Unable to read message.\n");
        return 1;
    }

    for (i = 0; message[i] != '\0'; i++) {
        if (message[i] == '\n' || message[i] == '\r') {
            message[i] = '\0';
            break;
        }
    }

    if (message[0] == '\0') {
        printf("Empty message. Nothing to send.\n");
        return 1;
    }

    channel = fopen(CHANNEL_FILE, "w");

    if (channel == NULL) {
        printf("Cannot open channel file.\n");
        return 1;
    }

    printf("========================================================\n");
    printf("      OSI SENDER - HAMMING CODE ERROR CORRECTION\n");
    printf("========================================================\n");

    for (i = 0; message[i] != '\0'; i++) {
        pdu[0] = '\0';

        printf("\n--- Encapsulating character '%c' ---\n",
               message[i]);

        applicationLayer((unsigned char)message[i], pdu);

        transportLayer(pdu, sourcePort, destinationPort);

        if (!networkLayer(pdu, sourceIP, destinationIP)) {
            fclose(channel);
            return 1;
        }

        if (!dataLinkLayer(pdu, sourceMAC, destinationMAC)) {
            fclose(channel);
            return 1;
        }

        fprintf(channel, "%s\n", pdu);

        printf(" [CHANNEL]     Frame written to %s\n",
               CHANNEL_FILE);
    }

    fclose(channel);

    printf("\nSender completed successfully.\n");

    return 0;
}

