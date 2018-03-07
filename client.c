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


    unsigned int client_isn = 2048; // needs randomize
    unsigned int server_isn = 0;
    unsigned int ackNum = 0;
    unsigned int synNum = 1;

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
    set4Bytes(buf, SEQ_NUM, client_isn);

    /* send the message to the server */
    serverlen = sizeof(serveraddr);
    n = sendto(sockfd, buf, strlen(buf), 0, &serveraddr, serverlen);
    printf("Sending packet %d %s\n", seqNum, message)
    if (n < 0) 
      error("ERROR in sendto");
    
    /* print the server's reply */

    while(1) {

        n = recvfrom(sockfd, buf, strlen(buf), MSG_DONTWAIT, &serveraddr, &serverlen);
        if (n >= 0) {
          printf("Receiving packet %d", get4Byets(buf, SEQ_NUM));
          synNum = getBit(buf, SYN);
          if (synNum == 1)
            
        server_isn = get4Bytes(buf, SEQ_NUM);
        ackNum = get4Bytes(buf, ACK_NUM);
    }

    return 0;
}
