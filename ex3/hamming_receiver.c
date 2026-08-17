#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHANNEL_FILE "hamming_channel.txt"

#define BUFFER_SIZE 1000
#define MESSAGE_SIZE 2000

/* Must match the sender */
#define LINK_FRAME_BITS 200
#define PARITY_BITS 8
#define CODEWORD_BITS (LINK_FRAME_BITS + PARITY_BITS)   /* 208 */

/* Layer field sizes */
#define MAC_HEADER_BITS 96
#define IP_HEADER_BITS 64
#define PORT_HEADER_BITS 32
#define DATA_BITS 8

/* ---------------- Binary Utilities ---------------- */

unsigned int binaryToDecimal(const char binary[], int bits) {
    unsigned int value;
    int i;

    value = 0;

    for (i = 0; i < bits; i++) {
        value = value * 2 + (binary[i] - '0');
    }

    return value;
}

int isBinaryString(const char binary[]) {
    int i;

    for (i = 0; binary[i] != '\0'; i++) {
        if (binary[i] != '0' && binary[i] != '1') {
            return 0;
        }
    }

    return 1;
}

void binaryToIP(const char binary[], char ip[]) {
    unsigned int a, b, c, d;

    a = binaryToDecimal(binary, 8);
    b = binaryToDecimal(binary + 8, 8);
    c = binaryToDecimal(binary + 16, 8);
    d = binaryToDecimal(binary + 24, 8);

    sprintf(ip, "%u.%u.%u.%u", a, b, c, d);
}

void binaryToMAC(const char binary[], char mac[]) {
    unsigned int b1, b2, b3, b4, b5, b6;

    b1 = binaryToDecimal(binary, 8);
    b2 = binaryToDecimal(binary + 8, 8);
    b3 = binaryToDecimal(binary + 16, 8);
    b4 = binaryToDecimal(binary + 24, 8);
    b5 = binaryToDecimal(binary + 32, 8);
    b6 = binaryToDecimal(binary + 40, 8);

    sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",
            b1, b2, b3, b4, b5, b6);
}

/* ---------------- Hamming Decoder ---------------- */

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
   Syndrome is the XOR of every position holding a '1'.

   Syndrome = 0            -> no error
   Syndrome <= length      -> single-bit error at that position
   Syndrome  > length      -> uncorrectable error
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
   Returns:
       0 -> uncorrectable
       1 -> clean frame
       2 -> single-bit error corrected
*/
int hammingDecode(char codeword[], int *errorPosition) {
    int length;
    int syndrome;

    length = strlen(codeword);
    syndrome = calculateSyndrome(codeword);

    *errorPosition = syndrome;

    if (syndrome == 0) {
        return 1;
    }

    if (syndrome <= length) {
        if (codeword[syndrome - 1] == '0') {
            codeword[syndrome - 1] = '1';
        } else {
            codeword[syndrome - 1] = '0';
        }

        return 2;
    }

    return 0;
}

/*
   Removes the parity bits and keeps only the data bits.
*/
void hammingExtractData(const char codeword[], char data[]) {
    int length;
    int i;
    int j;

    length = strlen(codeword);
    j = 0;

    for (i = 1; i <= length; i++) {
        if (!isPowerOfTwo(i)) {
            data[j] = codeword[i - 1];
            j = j + 1;
        }
    }

    data[j] = '\0';
}

/* ---------------- Data Link Receiver ---------------- */

int dataLinkReceiver(char pdu[]) {
    char linkFrame[BUFFER_SIZE];
    char destinationMACBinary[49];
    char sourceMACBinary[49];
    char destinationMAC[32];
    char sourceMAC[32];

    int actualBits;
    int status;
    int errorPosition;

    actualBits = strlen(pdu);

    printf(" [LINK LAYER]  Received frame: %d bits\n", actualBits);

    if (!isBinaryString(pdu)) {
        printf(" [LINK LAYER]  Non-binary characters found. DROP\n");
        return 0;
    }

    if (actualBits != CODEWORD_BITS) {
        printf(" [LINK LAYER]  Expected %d bits but received %d. DROP\n",
               CODEWORD_BITS, actualBits);
        return 0;
    }

    /* Hamming decoding and single-bit correction */
    status = hammingDecode(pdu, &errorPosition);

    printf(" [HAMMING]     Syndrome value: %d\n", errorPosition);

    if (status == 0) {
        printf(" [HAMMING]     Syndrome points outside the frame.\n");
        printf(" [HAMMING]     Uncorrectable error. DROP\n");
        return 0;
    }

    if (status == 1) {
        printf(" [HAMMING]     No errors detected.\n");
    }

    if (status == 2) {
        printf(" [HAMMING]     Single-bit error at position %d.\n",
               errorPosition);

        if (isPowerOfTwo(errorPosition)) {
            printf(" [HAMMING]     Error was in a parity bit.\n");
        } else {
            printf(" [HAMMING]     Error was in a data bit.\n");
        }

        printf(" [HAMMING]     Bit flipped back. Error corrected.\n");
        printf(" [HAMMING]     Recheck syndrome: %d\n",
               calculateSyndrome(pdu));
    }

    /* Remove parity bits to recover the 200-bit link frame */
    hammingExtractData(pdu, linkFrame);

    printf(" [HAMMING]     Extracted data: %d bits\n",
           (int)strlen(linkFrame));

    if ((int)strlen(linkFrame) != LINK_FRAME_BITS) {
        printf(" [LINK LAYER]  Data length mismatch. DROP\n");
        return 0;
    }

    /* Decode MAC addresses */
    strncpy(destinationMACBinary, linkFrame, 48);
    destinationMACBinary[48] = '\0';

    strncpy(sourceMACBinary, linkFrame + 48, 48);
    sourceMACBinary[48] = '\0';

    binaryToMAC(destinationMACBinary, destinationMAC);
    binaryToMAC(sourceMACBinary, sourceMAC);

    printf(" [LINK LAYER]  Destination MAC: %s\n", destinationMAC);
    printf(" [LINK LAYER]  Source MAC: %s\n", sourceMAC);

    /* Strip the 96-bit MAC header */
    strcpy(pdu, linkFrame + MAC_HEADER_BITS);

    printf(" [LINK LAYER]  Removed MACs: %d bits remain\n",
           (int)strlen(pdu));

    return 1;
}

/* ---------------- Network Receiver ---------------- */

int networkReceiver(char pdu[]) {
    char destinationIPBinary[33];
    char sourceIPBinary[33];
    char destinationIP[32];
    char sourceIP[32];
    char temporary[BUFFER_SIZE];

    int length;

    length = strlen(pdu);

    if (length < IP_HEADER_BITS) {
        printf(" [NET LAYER]   Missing IP headers. DROP\n");
        return 0;
    }

    strncpy(destinationIPBinary, pdu, 32);
    destinationIPBinary[32] = '\0';

    strncpy(sourceIPBinary, pdu + 32, 32);
    sourceIPBinary[32] = '\0';

    binaryToIP(destinationIPBinary, destinationIP);
    binaryToIP(sourceIPBinary, sourceIP);

    printf(" [NET LAYER]   Destination IP: %s\n", destinationIP);
    printf(" [NET LAYER]   Source IP: %s\n", sourceIP);

    strcpy(temporary, pdu + IP_HEADER_BITS);
    strcpy(pdu, temporary);

    printf(" [NET LAYER]   Removed IPs: %d bits remain\n",
           (int)strlen(pdu));

    return 1;
}

/* ---------------- Transport Receiver ---------------- */

int transportReceiver(char pdu[]) {
    char sourcePortBinary[17];
    char destinationPortBinary[17];
    char temporary[BUFFER_SIZE];

    unsigned int sourcePort;
    unsigned int destinationPort;

    int length;

    length = strlen(pdu);

    if (length < PORT_HEADER_BITS) {
        printf(" [TRANS LAYER] Missing port headers. DROP\n");
        return 0;
    }

    strncpy(sourcePortBinary, pdu, 16);
    sourcePortBinary[16] = '\0';

    strncpy(destinationPortBinary, pdu + 16, 16);
    destinationPortBinary[16] = '\0';

    sourcePort = binaryToDecimal(sourcePortBinary, 16);
    destinationPort = binaryToDecimal(destinationPortBinary, 16);

    printf(" [TRANS LAYER] Source port: %u\n", sourcePort);
    printf(" [TRANS LAYER] Destination port: %u\n", destinationPort);

    strcpy(temporary, pdu + PORT_HEADER_BITS);
    strcpy(pdu, temporary);

    printf(" [TRANS LAYER] Removed ports: %d bits remain\n",
           (int)strlen(pdu));

    return 1;
}

/* ---------------- Application Receiver ---------------- */

int applicationReceiver(char pdu[], char message[], int *index) {
    char character;

    if ((int)strlen(pdu) < DATA_BITS) {
        printf(" [APP LAYER]   Missing application data. DROP\n");
        return 0;
    }

    if (*index >= MESSAGE_SIZE - 1) {
        printf(" [APP LAYER]   Message buffer full. DROP\n");
        return 0;
    }

    character = (char)binaryToDecimal(pdu, DATA_BITS);

    message[*index] = character;
    *index = *index + 1;
    message[*index] = '\0';

    printf(" [APP LAYER]   Recovered character: '%c'\n", character);

    return 1;
}

/* ---------------- Main ---------------- */

int main(void) {
    FILE *channel;

    char pdu[BUFFER_SIZE];
    char recoveredMessage[MESSAGE_SIZE];

    int messageIndex;
    int frameNumber;

    channel = fopen(CHANNEL_FILE, "r");

    if (channel == NULL) {
        printf("Cannot open channel file. Run the sender first.\n");
        return 1;
    }

    recoveredMessage[0] = '\0';
    messageIndex = 0;
    frameNumber = 0;

    printf("========================================================\n");
    printf("     OSI RECEIVER - HAMMING CODE ERROR CORRECTION\n");
    printf("========================================================\n");

    while (fscanf(channel, "%999s", pdu) == 1) {
        frameNumber++;

        printf("\n--- Decapsulating frame %d ---\n", frameNumber);

        if (!dataLinkReceiver(pdu)) {
            printf(" [RECEIVER]    Frame dropped.\n");
            continue;
        }

        if (!networkReceiver(pdu)) {
            printf(" [RECEIVER]    Frame dropped.\n");
            continue;
        }

        if (!transportReceiver(pdu)) {
            printf(" [RECEIVER]    Frame dropped.\n");
            continue;
        }

        if (!applicationReceiver(pdu,
                                 recoveredMessage,
                                 &messageIndex)) {
            printf(" [RECEIVER]    Frame dropped.\n");
            continue;
        }

        printf(" [RECEIVER]    Frame accepted.\n");
    }

    fclose(channel);

    printf("\n========================================================\n");
    printf(" Final reassembled message: \"%s\"\n", recoveredMessage);
    printf("========================================================\n");

    return 0;
}
