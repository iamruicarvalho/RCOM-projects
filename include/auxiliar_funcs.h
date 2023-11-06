#ifndef _AUX_FUNCS_H_
#define _AUX_FUNCS_H_

#include "link_layer.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>

#define FLAG       0x7E
#define A_SET      0x03
#define C_SET      0x03
#define BCC1_SET   (A_SET ^ C_SET)
#define A_UA       0x01
#define C_UA       0x07
#define BCC1_UA    (A_UA ^ C_UA)
#define A_ER       0x03
#define A_RE       0x01
#define A_DISC     0x03
#define C_DISC     0x0B
#define BCC1_DISC  (A_DISC ^ C_DISC)
#define C_N(Ns)    (Ns << 6)
#define ESC        0x7D
#define C_RR(Nr)   ((Nr << 7) | 0x05)
#define C_REJ(Nr)  ((Nr << 7) | 0x01)

typedef enum
{
   START_TX,
   FLAG_RCV,
   A_RCV,
   C_RCV,
   BCC1_OK,
   STOP_R,
   DATA_FOUND_ESC,
   READING_DATA,
   DISCONNECTED,
   BCC2_OK
} LinkLayerState;

int linkTx(LinkLayer connection);
int linkRx(LinkLayer connection);
void alarmHandler(int signal);
int makeConnection(const char* serialPort);
int sendSupervisionFrame(unsigned char A, unsigned char C);
unsigned char* getControlPacket(const unsigned int c, const char* filename, long int length, unsigned int* size);
unsigned char* getDataPacket(unsigned char sequence, unsigned char *data, int dataSize, int *packetSize);
unsigned char* parseControlPacket(unsigned char* packet, int size, unsigned long int *fileSize);
void parseDataPacket(const unsigned char* packet, const unsigned int packetSize, unsigned char* buffer);
unsigned char* getData(FILE* fd, long int fileLength);

#endif
