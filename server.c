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
void error(char *msg) {
  perror(msg);
  exit(1);
}
// Networking stuff
int sockfd;
struct sockaddr_in clientaddr; /* client addr */
unsigned int clientlen = sizeof(clientaddr);
struct hostent *hostp; /* client host info */
char *hostaddrp; /* dotted decimal host addr string */

// Global buffer I use for input and output
char buf[BUFSIZE];

const int PACKET_SIZE = 1024;
const int HEADER_SIZE = 20;
const int PAYLOAD_SIZE = PACKET_SIZE - HEADER_SIZE;
unsigned int windowSize = 5 * PACKET_SIZE;
const int MAX_SEQ = 30 * PACKET_SIZE;
long data[30][PAYLOAD_SIZE];
char dataSet[30];
unsigned long timeouts[30];
long TIMEOUT = 500;

void updateData(int oldSeq, int currSeq, FILE* fp) {
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
      fread(data[index], 1, PAYLOAD_SIZE, fp);
      dataSet[index] = 1;
    }
  }
}

int inWindow(int currSeq, int seq) {
  return (seq >= currSeq && (seq - currSeq) > 0 && (seq - currSeq) < windowSize) ||
         (seq < currSeq && (seq + MAX_SEQ - currSeq) > 0 && (seq + MAX_SEQ - currSeq) < windowSize);
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
  set4Bytes(buf, SEQ_NUM, seq);
  memcpy(buf+HEADER_SIZE, data[index], PAYLOAD_SIZE);
  int n = sendto(sockfd, buf, BUFSIZE, 0, 
       (struct sockaddr *) &clientaddr, clientlen);
  if (n < 0) 
    error("Error in sendto");
}

int main(int argc, char **argv) {
  /* 
   * check command line arguments 
   */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  srand(time(NULL));

  int portno = atoi(argv[1]);

  /* 
   * socket: create the parent socket 
   */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");

  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  int optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
	     (const void *)&optval , sizeof(int));

  /*
   * build the server's Internet address
   */
  struct sockaddr_in serveraddr; /* server's addr */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);

  /* 
   * bind: associate the parent socket with a port 
   */
  if (bind(sockfd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  /* 
   * main loop: wait for a datagram, then echo it
   */
  
  int cumSeq = 0;
  int currSeq;
  unsigned int randNum = rand() % 30;
  char filename[128];
  FILE* fp;
  while(1) {
    bzero(buf, BUFSIZE);
    int n = recvfrom(sockfd, buf, BUFSIZE, 0,
      (struct sockaddr *) &clientaddr, &clientlen);
    if (n > 0) {
      if (getBit(buf, SYN) && !getBit(buf, ACK)) {
        setBit(buf, ACK, 1);
        int temp = get4Bytes(buf, SEQ_NUM);
        set4Bytes(buf, ACK_NUM, temp + 1);
        set4Bytes(buf, SEQ_NUM, randNum);
        n = sendto(sockfd, buf, BUFSIZE, 0, 
         (struct sockaddr *) &clientaddr, clientlen);
        printf("Sending packet %d %d SYN\n", SEQ_NUM, windowSize);
        if (n < 0) {
          error("Coult not send syn-ack.");
        }
        strcpy(filename, buf+HEADER_SIZE);
      }
      else if (getBit(buf, ACK)) {
        currSeq = get4Bytes(buf, ACK_NUM);
        printf("Receiving packet %d\n", currSeq);
        assert(currSeq == randNum+1);
        break;
      } 
    }
  }

  // Handshake complete

  fp = fopen(filename, "r");
  if (fp == NULL) {
    error("Could not open file.");
  }
  for (int i = 0; i < 30; ++i) {
    dataSet[i] = 0;
    timeouts[i] = ULONG_MAX;
  }

  int done = false;
  while (!done) {
    /*
     * recvfrom: receive a UDP datagram from a client
     */
    bzero(buf, BUFSIZE);
    int n = recvfrom(sockfd, buf, BUFSIZE, MSG_DONTWAIT,
		 (struct sockaddr *) &clientaddr, &clientlen);
    if (n > 0) {
      if (getBit(buf, SYN) && !getBit(buf, ACK)) {
        setBit(buf, ACK, 1);
        set4Bytes(buf, ACK_NUM, get4Bytes(buf, SEQ_NUM) + 1);
        set4Bytes(buf, SEQ_NUM, 0);
      }
      else if (getBit(buf, ACK)) {
        int seqNum = get4Bytes(buf, SEQ_NUM);
        timeouts[seqNum / PACKET_SIZE] = 0;
        int oldSeq = currSeq;
        int tempSeq = get4Bytes(buf, ACK_NUM);
        if (inWindow(currSeq, tempSeq)) {
          currSeq = tempSeq;
          updateData(oldSeq, currSeq, fp);
          if (!dataSet[currSeq / PACKET_SIZE]) {
            done = true;
          }
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

  // TODO: Handle shutdown procedure.

  return 0;
}