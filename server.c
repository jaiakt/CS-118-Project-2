/* 
 * udpserver.c - A simple UDP echo server 
 * usage: udpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <limits.h>
#include "utilities.h"

#define BUFSIZE 1024

/*
 * error - wrapper for perror
 */
void error(char * msg) {
    perror(msg);
    exit(1);
}
    // Networking stuff
int sockfd;
struct sockaddr_in clientaddr; /* client addr */
unsigned int clientlen = sizeof(clientaddr);
struct hostent * hostp; /* client host info */
char * hostaddrp; /* dotted decimal host addr string */

// Global buffer I use for input and output
char buf[BUFSIZE];

unsigned int windowSize = 5 * PACKET_SIZE;
char data[30][PAYLOAD_SIZE];
char dataSet[30];
unsigned long timeouts[30];
long TIMEOUT = 500;
unsigned int lastPacketSeq = -1;
unsigned int lastPacketBytes = -1;

void updateData(int oldSeq, int currSeq, FILE * fp) {
    for (int i = oldSeq; i != currSeq; i = (i + PACKET_SIZE) % MAX_SEQ) {
        dataSet[i / PACKET_SIZE] = 0;
    }
    for (int i = 0; i < windowSize; i += PACKET_SIZE) {
        if (feof(fp)) {
            break;
        }
        int seq = (currSeq + i) % MAX_SEQ;
        int index = seq / PACKET_SIZE;
        if (dataSet[index] == 0) {
            int bytes = fread(data[index], 1, PAYLOAD_SIZE, fp);
            if (feof(fp)) {
                lastPacketSeq = seq;
                lastPacketBytes = bytes;
            }
            dataSet[index] = 1;
        }
    }
}

void sendPacket(int seq, int retransmit) {
    int index = seq / PACKET_SIZE;
    if (!dataSet[index]) {
        return;
    }
    char retransmitStr[] = " Retransmission";
    if (!retransmit) {
        strcpy(retransmitStr, "");
    }
    printf("Sending packet %d %d%s\n", seq, windowSize, retransmitStr);
    bzero(buf, BUFSIZE);
    set4Bytes(buf, SEQ_NUM, seq);
    set2Bytes(buf, WINDOW, windowSize / PACKET_SIZE);
    memcpy(buf + HEADER_SIZE, data[index], PAYLOAD_SIZE);
    if (seq != lastPacketSeq) {
        int n = sendto(sockfd, buf, BUFSIZE, 0,
            (struct sockaddr * ) & clientaddr, clientlen);
        if (n < 0) {
            error("Error in sendto");
        }
    } else {
        int n = sendto(sockfd, buf, HEADER_SIZE + lastPacketBytes, 0,
            (struct sockaddr * ) & clientaddr, clientlen);
        if (n < 0) {
            error("Error in sendto");
        }
    }
}

int main(int argc, char * * argv) {
    /* 
     * check command line arguments 
     */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    srand(time(NULL));

    int portno = atoi(argv[1]);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
        (const void * ) & optval, sizeof(int));

    struct sockaddr_in serveraddr; /* server's addr */
    bzero((char * ) & serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short) portno);

    if (bind(sockfd, (struct sockaddr * ) & serveraddr,
            sizeof(serveraddr)) < 0)
        error("ERROR on binding");

    int cumSeq = 0;
    int currSeq;
    unsigned int randNum = rand() % 30;
    char filename[128];
    FILE * fp;
    while (1) {
        bzero(buf, BUFSIZE);
        int n = recvfrom(sockfd, buf, BUFSIZE, 0,
            (struct sockaddr * ) & clientaddr, & clientlen);
        if (n > 0) {
            if (getBit(buf, SYN) && !getBit(buf, ACK)) {
                setBit(buf, ACK, 1);
                int temp = get4Bytes(buf, SEQ_NUM);
                set4Bytes(buf, ACK_NUM, temp + 1);
                set4Bytes(buf, SEQ_NUM, randNum);
                n = sendto(sockfd, buf, BUFSIZE, 0,
                    (struct sockaddr * ) & clientaddr, clientlen);
                printf("Sending packet %d %d SYN\n", SEQ_NUM, windowSize);
                if (n < 0) {
                    error("Coult not send syn-ack.");
                }
                strcpy(filename, buf + HEADER_SIZE);
            } else if (getBit(buf, ACK)) {
                currSeq = get4Bytes(buf, ACK_NUM);
                printf("Receiving packet %d\n", currSeq);
                assert(currSeq == randNum + 1);
                break;
            }
        }
    }

    // Handshake complete

    fp = fopen(filename, "rb");
    if (fp == NULL) {
        error("Could not open file.");
    }
    for (int i = 0; i < 30; ++i) {
        dataSet[i] = 0;
        timeouts[i] = ULONG_MAX;
    }

    updateData(currSeq, currSeq, fp);

    int done = 0;
    while (!done) {
        bzero(buf, BUFSIZE);
        int n = recvfrom(sockfd, buf, BUFSIZE, MSG_DONTWAIT,
            (struct sockaddr * ) & clientaddr, & clientlen);
        if (n > 0 && getBit(buf, ACK)) {
            int seqNum = get4Bytes(buf, SEQ_NUM);
            timeouts[seqNum / PACKET_SIZE] = ULONG_MAX;
            int tempSeq = get4Bytes(buf, ACK_NUM);
            if (currSeq == tempSeq) {
                bzero(buf, BUFSIZE);
                set4Bytes(buf, SEQ_NUM, currSeq);
                set2Bytes(buf, WINDOW, windowSize / PACKET_SIZE);
                memcpy(buf + HEADER_SIZE, data[currSeq / PACKET_SIZE], PAYLOAD_SIZE);
                if (currSeq != lastPacketSeq) {
                    int n = sendto(sockfd, buf, BUFSIZE, 0,
                        (struct sockaddr * ) &clientaddr, clientlen);
                    if (n < 0) {
                        error("Error in sendto");
                    }
                }
                else {
                    int n = sendto(sockfd, buf, HEADER_SIZE + lastPacketBytes, 0,
                        (struct sockaddr * ) &clientaddr, clientlen);
                    if (n < 0) {
                        error("Error in sendto");
                    }
                }
            }
            int oldSeq = currSeq;
            if (inWindow(currSeq, tempSeq, windowSize)) {
                currSeq = tempSeq;
                updateData(oldSeq, currSeq, fp);
                if (!dataSet[currSeq / PACKET_SIZE]) {
                    done = 1;
                }
            }
        }

        // Handle timeouts

        for (int i = 0; i < windowSize; i += PACKET_SIZE) {
            int seq = (currSeq + i) % MAX_SEQ;
            long currentTime = getCurrentTime();
            if (timeouts[seq / PACKET_SIZE] < currentTime || timeouts[seq / PACKET_SIZE] == ULONG_MAX) {
                int retransmit = timeouts[seq / PACKET_SIZE] < currentTime;
                sendPacket(seq, retransmit);
                timeouts[seq / PACKET_SIZE] = currentTime + TIMEOUT;
            }
        }
    }

    fclose(fp);

    printf("Sending packet %d FIN\n", currSeq);
    setBit(buf, FIN, 1);
    int n = sendto(sockfd, buf, BUFSIZE, 0,
        (struct sockaddr * ) & clientaddr, clientlen);
    long timeout = getCurrentTime() + TIMEOUT;
    long msl = getCurrentTime();

    while (1) {
        bzero(buf, BUFSIZE);
        int n = recvfrom(sockfd, buf, BUFSIZE, MSG_DONTWAIT,
            (struct sockaddr * ) & clientaddr, & clientlen);
        if (n > 0 && getBit(buf, ACK) == 1) {
            msl = getCurrentTime() - msl;
            break;
        }
        else if (n > 0 && getBit(buf, FIN) == 1) {
            msl = getCurrentTime() - msl;
            printf("Sending packet %d ACK\n", currSeq);
            setBit(buf, FIN, 1);
            int n = sendto(sockfd, buf, BUFSIZE, 0,
                (struct sockaddr * ) & clientaddr, clientlen);
            break;
        }
    }

    waitTill = getCurrentTime() + 2 * msl;

    while (1) {
        int n = recvfrom(sockfd, buf, BUFSIZE, MSG_DONTWAIT,
            (struct sockaddr * ) & clientaddr, & clientlen);
        if (n > 0 && getBit(buf, FIN) == 1) {
            printf("Sending packet %d ACK\n", currSeq);
            setBit(buf, ACK, 1);
            int n = sendto(sockfd, buf, BUFSIZE, 0,
                (struct sockaddr * ) & clientaddr, clientlen);
            waitTill = getCurrentTime() + 2 * msl;
        }
        if (waitTill < getCurrentTime()) {
            break;
        }
    }

    return 0;
}