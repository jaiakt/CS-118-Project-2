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
#include "utilities.h"

#define BUFSIZE 1024

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

int main(int argc, char **argv) {
  /* 
   * check command line arguments 
   */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  int portno = atoi(argv[1]);

  /* 
   * socket: create the parent socket 
   */
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
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
  struct sockaddr_in clientaddr; /* client addr */
  unsigned int clientlen = sizeof(clientaddr);
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE]; /* message buf */
  char *hostaddrp; /* dotted decimal host addr string */
  const int PACKET_SIZE = 1024;
  const int HEADER_SIZE = 20;
  const int PAYLOAD_SIZE = PACKET_SIZE - HEADER_SIZE;
  int windowSize = 5 * PACKET_SIZE;
  const int MAX_SEQ = 30 * PACKET_SIZE;
  long timeouts[MAX_SEQ];
  for (int i = 0; i < MAX_SEQ; ++i) {
    timeouts[i] = 0;
  }
  int cumSeq = 0;
  int currSeq;
  while (1) {
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
        currSeq = get4Bytes(buf, ACK_NUM);
      }
    }
    // HANDLE TIMEOUTS
    // We need to respond to packet
      /* 
       * gethostbyaddr: determine who sent the datagram
       */
      hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
          sizeof(clientaddr.sin_addr.s_addr), AF_INET);
      if (hostp == NULL)
        error("ERROR on gethostbyaddr");
      hostaddrp = inet_ntoa(clientaddr.sin_addr);
      if (hostaddrp == NULL)
        error("ERROR on inet_ntoa\n");
      printf("server received datagram from %s (%s)\n", 
       hostp->h_name, hostaddrp);
      printf("server received %lu/%d bytes: %s\n", strlen(buf), n, buf);
      
      /* 
       * sendto: echo the input back to the client 
       */
      n = sendto(sockfd, buf, strlen(buf), 0, 
           (struct sockaddr *) &clientaddr, clientlen);
      if (n < 0) 
        error("ERROR in sendto");
  }
}