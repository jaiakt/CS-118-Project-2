/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include "utilities.h"

#define BUFSIZE 1024

/* 
 * error - wrapper for perror
 */
void error(char * msg) {
    perror(msg);
    exit(0);
}

long data[30][PAYLOAD_SIZE];
char dataSet[30];

int main(int argc, char * * argv) {
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent * server;
    char * hostname;
    char * filename;
    char buf[BUFSIZE]; // 20 bytes for "TCP" header.
    unsigned int lastPacketSeq = -1;
    unsigned int lastPacketBytes = -1;
    FILE * fp;

    /* check command line arguments */
    if (argc != 4) {
        fprintf(stderr, "usage: %s <hostname> <port> <filename>\n", argv[0]);
        exit(0);
    }
    srand(time(NULL));
    hostname = argv[1];
    portno = atoi(argv[2]);
    filename = argv[3];

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char * ) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char * ) server -> h_addr,
        (char * ) &serveraddr.sin_addr.s_addr, server -> h_length);
    serveraddr.sin_port = htons(portno);

    unsigned int client_isn = 0; // needs randomize // initial seq num
    unsigned int server_isn = 0;
    unsigned int ackNum = 0;
    unsigned int synNum = 1;
    unsigned int seqNum = client_isn;
    unsigned int nextSeqNum = 0;

    // Listen for server return msg
    // while ( !msgRcvd ) {
    //       if (currTime >= lastTime + 2 * timeout)
    //              sendSyn
    //       listen for server msg 
    //       
    // }
    /* user send connection request */
    bzero(buf, BUFSIZE);
    setBit(buf, SYN, 1);
    set4Bytes(buf, SEQ_NUM, seqNum);
    strcpy(buf + HEADER_SIZE, filename);

    /* send the message to the server */
    serverlen = sizeof(serveraddr);
    n = sendto(sockfd, buf, BUFSIZE, 0, (struct sockaddr * ) & serveraddr, serverlen);
    printf("Sending packet %d SYN\n", seqNum);
    if (n < 0)
        error("ERROR in sendto");

    /* print the server's reply */
    unsigned long TIMEOUT = 500;
    unsigned long timeout = getCurrentTime() + TIMEOUT;
    char established = 0;

    while (1) {
        bzero(buf, BUFSIZE);
        n = recvfrom(sockfd, buf, BUFSIZE, MSG_DONTWAIT, (struct sockaddr * ) & serveraddr, (socklen_t * ) & serverlen);
        if (n > 0) {
            if (getBit(buf, FIN) == 1) {
                printf("Received packet FIN\n");
                printf("Sending packet ACK\n", seqNum);
                setBit(buf, ACK, 1);
                int n = sendto(sockfd, buf, BUFSIZE, 0,
                    (struct sockaddr * ) & serveraddr, serverlen);
                break;
            }
            else if (getBit(buf, SYN) == 1) {
                server_isn = get4Bytes(buf, SEQ_NUM);
                printf("Receiving packet %d\n", server_isn);

                int ackBit = getBit(buf, ACK);
                if (ackBit == 1)
                    ackNum = get4Bytes(buf, ACK_NUM);
                else
                    error("Unable to establish handshake. Bad ACK bit.\n");

                // Create final client response msg with single ACK packet.
                bzero(buf, BUFSIZE);
                setBit(buf, ACK, 1);
                set4Bytes(buf, ACK_NUM, server_isn + 1);
                nextSeqNum = server_isn + 1;
                n = sendto(sockfd, buf, BUFSIZE, 0, (struct sockaddr * ) & serveraddr, serverlen);
                printf("Sending packet %d %s\n", nextSeqNum, "ACK");
            }
            else {
                if (!established) {
                    established = 1;
                    fp = fopen("received.data", "wb");
                    for (int i = 0; i < 30; ++i) {
                        dataSet[i] = 0;
                    }
                }
                unsigned int inSeqNum = get4Bytes(buf, SEQ_NUM);
                printf("Receiving packet %d\n", inSeqNum);
                if (n < PACKET_SIZE) {
                    lastPacketSeq = inSeqNum;
                    lastPacketBytes = n - HEADER_SIZE;
                }
                unsigned int packetNum = inSeqNum / 1024;
                unsigned int windowSize = get2Bytes(buf, WINDOW);
                if (inSeqNum == nextSeqNum) {
                    fwrite(buf + HEADER_SIZE, sizeof(char), n - HEADER_SIZE, fp);
                    nextSeqNum = nextSeqNum + PACKET_SIZE;
                    nextSeqNum %= MAX_SEQ;
                    while (dataSet[nextSeqNum / PACKET_SIZE]) {
                        if (nextSeqNum != lastPacketSeq) {
                            fwrite(data[nextSeqNum / PACKET_SIZE], sizeof(char), PAYLOAD_SIZE, fp);
                        } else {
                            fwrite(data[nextSeqNum / PACKET_SIZE], sizeof(char), lastPacketBytes, fp);
                        }
                        dataSet[nextSeqNum / PACKET_SIZE] = 0;
                        nextSeqNum = nextSeqNum + PACKET_SIZE;
                        nextSeqNum %= MAX_SEQ;
                    }
                } else if (inWindow(nextSeqNum, inSeqNum, windowSize)) {
                    memcpy(data[packetNum], buf + HEADER_SIZE, n - HEADER_SIZE);
                    dataSet[packetNum] = 1;
                }
                printf("Sending packet %d\n", nextSeqNum);
                set4Bytes(buf, ACK_NUM, nextSeqNum);
                setBit(buf, ACK, 1);
                n = sendto(sockfd, buf, BUFSIZE, 0, (struct sockaddr * ) & serveraddr, serverlen);
                if (n < 0) {
                    error("Error in sendto.");
                }
            }
        }
        if (!established && timeout < getCurrentTime()) {
            // Retransmit
            bzero(buf, BUFSIZE);
            setBit(buf, SYN, 1);
            set4Bytes(buf, SEQ_NUM, seqNum);
            strcpy(buf + HEADER_SIZE, filename);
            n = sendto(sockfd, buf, BUFSIZE, 0, (struct sockaddr * ) & serveraddr, serverlen);
            printf("Sending packet %d Retransmission SYN\n", seqNum);
            if (n < 0)
                error("ERROR in sendto");
            timeout = getCurrentTime() + TIMEOUT;
        }
    }

    fclose(fp);

    timeout = getCurrentTime() + TIMEOUT/5;
    printf("Sending packet FIN\n");
    setBit(buf, FIN, 1);
    n = sendto(sockfd, buf, BUFSIZE, 0,
        (struct sockaddr * ) & serveraddr, serverlen);

    while (1) {
        int n = recvfrom(sockfd, buf, BUFSIZE, MSG_DONTWAIT,
            (struct sockaddr * ) & serveraddr, & serverlen);
        if (n > 0 && getBit(buf, ACK) == 1) {
            printf("Received packet ACK\n");
            printf("Now shutting down...\n");
            break;
        }
        if (timeout < getCurrentTime()) {
            printf("Sending packet FIN\n");
            setBit(buf, FIN, 1);
            n = sendto(sockfd, buf, BUFSIZE, 0,
                (struct sockaddr * ) & serveraddr, serverlen);
            timeout = getCurrentTime() + TIMEOUT/5;
        }
    }

    return 0;
}
