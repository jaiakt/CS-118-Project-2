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

#define BUFSIZE 1024

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

const int SOURCE_PORT = 0;
const int DEST_PORT = 2;
const int SEQ_NUM = 4;
const int ACK_NUM = 8;
const int DATA_OFFSET = 12;
const int CONTROL = 13;
const int WINDOW = 14;
const int CHECKSUM = 16;
const int URGENT_PTR = 18;

// Flags
const int URG = 0;
const int ACK = 1;
const int PSH = 2;
const int RST = 3;
const int SYN = 4;
const int FIN = 5;

unsigned int get4Bytes(const char buf[], const int offset) {
  unsigned int* ptr = (unsigned int *) (buf + offset);
  return *ptr;
}

void set4Bytes(char buf[], const int offset, const unsigned int val) {
  unsigned int* ptr = (unsigned int *) (buf + offset);
  *ptr = val;
}

short get2Bytes(const char buf[], const int offset) {
  short* ptr = (short *) (buf + offset);
  return *ptr;
}

void set2Bytes(char buf[], const int offset, const short val) {
  short* ptr = (short *) (buf + offset);
  *ptr = val;
}

char getBit(const char buf[], const int bit) {
  unsigned char control = buf[CONTROL];
  return (control >> bit) & 1;
}

void setBit(char buf[], const int bit, const char val) {
  if (val){
    buf[CONTROL] |= (1 << bit);
  }
  else {
    buf[CONTROL] &= (0xFFFFFFFF ^ (1 << bit));
  }
}


int main(int argc, char **argv) {
  int sockfd; /* socket */
  int portno; /* port to listen on */
  int clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE]; /* message buf */
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */
  int windowSize = 5;
  int currSeq;

  /* 
   * check command line arguments 
   */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);

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
  optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
	     (const void *)&optval , sizeof(int));

  /*
   * build the server's Internet address
   */
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
  clientlen = sizeof(clientaddr);
  while (1) {
    /*
     * recvfrom: receive a UDP datagram from a client
     */
    bzero(buf, BUFSIZE);
    n = recvfrom(sockfd, buf, BUFSIZE, MSG_DONTWAIT,
		 (struct sockaddr *) &clientaddr, &clientlen);
    if (n > 0) {
      if (getBit(buf, SYN) && !getBit(buf, ACK)) {
        setBit(buf, ACK, 1);
      }
      else if (getBit(buf, ACK)) {
        int ackNum = 
      }

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
      printf("server received %d/%d bytes: %s\n", strlen(buf), n, buf);
      
      /* 
       * sendto: echo the input back to the client 
       */
      n = sendto(sockfd, buf, strlen(buf), 0, 
  	       (struct sockaddr *) &clientaddr, clientlen);
      if (n < 0) 
        error("ERROR in sendto");
    }
  }
}