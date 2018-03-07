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

#define BUFSIZE 20 

/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}


int main(int argc, char **argv) {
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char *filename;
    char buf[BUFSIZE]; // 20 bytes for "TCP" header.

    /* check command line arguments */
    if (argc != 4) {
       fprintf(stderr,"usage: %s <hostname> <port> <filename>\n", argv[0]);
       exit(0);
    }
    srand(time(NULL));
    hostname = argv[1];
    portno = atoi(argv[2]);
    filename = argv[3];

    printf("hostname: %s\nportno: %d\nfilename: %s\n", hostname, portno, filename);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);


    unsigned int client_isn = 0; // needs randomize // initial seq num
    unsigned int server_isn = 0;
    unsigned int ackNum = 0;
    unsigned int synNum = 1;
    unsigned int seqNum = client_isn;

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

    /* send the message to the server */
    serverlen = sizeof(serveraddr);
    n = sendto(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &serveraddr, serverlen);
    printf("Sending packet %d %s\n", seqNum, "SYN");
    if (n < 0) 
      error("ERROR in sendto");
    
    /* print the server's reply */

    while(1) {
      bzero(buf, BUFSIZE);
      n = recvfrom(sockfd, buf, BUFSIZE, MSG_DONTWAIT, (struct sockaddr *) &serveraddr, (socklen_t *) &serverlen);
      if (n > 0) {
        server_isn = get4Bytes(buf, SEQ_NUM);
        printf("Receiving packet %d\n", server_isn);
        int synBit = getBit(buf, SYN);
        if (synBit != 1)

          error("Unable to establish handshake. Bad SYN bit.\n");

        int ackBit = getBit(buf, ACK);
        if (ackBit == 1)
          ackNum = get4Bytes(buf, ACK_NUM);
        else
          error("Unable to establish handshake. Bad ACK bit.\n");
        
        // Create final client response msg with sungle ACK packet.
        bzero(buf, BUFSIZE);
        setBit(buf, ACK, 1);
        set4Bytes(buf, ACK_NUM, server_isn+1);
        n = sendto(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &serveraddr, serverlen);
        printf("Sending packet %d %s\n", ackNum, "ACK");
        if (n > 0) {
            break;
        }
      }     
    }
    // todo: Start receiving packets

    return 0;
}
