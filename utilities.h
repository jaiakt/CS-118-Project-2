#ifndef UTILITIES_H
#define UTILITIES_H

#include <time.h>

#define PACKET_SIZE 1024
#define HEADER_SIZE 20
#define PAYLOAD_SIZE PACKET_SIZE - HEADER_SIZE
#define MAX_SEQ 30 * PACKET_SIZE

// Header Offsets
#define SOURCE_PORT 0
#define DEST_PORT 2
#define SEQ_NUM 4
#define ACK_NUM 8
#define DATA_OFFSET 12
#define CONTROL 13
#define WINDOW 14
#define CHECKSUM 16
#define URGENT_PTR 18

// Flags (bit offsets)
#define URG 0
#define ACK 1
#define PSH 2
#define RST 3
#define SYN 4
#define FIN 5

unsigned int get4Bytes(const char buf[], const int offset) {
    unsigned int* ptr = (unsigned int *) (buf + offset);
    return *ptr;
}

void set4Bytes(char buf[], const int offset, const unsigned int val) {
    unsigned int* ptr = (unsigned int *) (buf + offset);
    *ptr = val;
}

short get2Bytes(const char buf[], const int offset) {
    unsigned short* ptr = (unsigned short *) (buf + offset);
    return *ptr;
}

void set2Bytes(char buf[], const int offset, const unsigned short val) {
    unsigned short* ptr = (unsigned short *) (buf + offset);
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

unsigned long getCurrentTime() {
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);

    return (spec.tv_sec) * 1000 + (spec.tv_nsec) / 1000000 ;
}

void printHeader(char* buf) {
    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 4; ++j) {
            printf("%hhx\t", buf[i*4 + j]);
        }
        printf("\n");
    }
}

int inWindow(int currSeq, int seq, int windowSize) {
    return (seq >= currSeq && (seq - currSeq) > 0 && (seq - currSeq) <= windowSize) ||
                 (seq < currSeq && (seq + MAX_SEQ - currSeq) > 0 && (seq + MAX_SEQ - currSeq) <= windowSize);
}

#endif