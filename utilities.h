#ifndef UTILITIES_H
#define UTILITIES_H
// Header Offsets
const int SOURCE_PORT = 0;
const int DEST_PORT = 2;
const int SEQ_NUM = 4;
const int ACK_NUM = 8;
const int DATA_OFFSET = 12;
const int CONTROL = 13;
const int WINDOW = 14;
const int CHECKSUM = 16;
const int URGENT_PTR = 18;

// Flags (bit offsets)
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
#endif