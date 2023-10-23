#ifndef _AUX_FUNCS_H_
#define _AUX_FUNCS_H_

#include "link_layer.h"

int linkTx(LinkLayer connection);
int linkRx(LinkLayer connection);
void alarmHandler(int signal);
int makeConnection(const char* serialPort);
int sendSupervisionFrame(int fd, unsigned char A, unsigned char C);
unsigned char* parseControlPacket(unsigned char* packet, int size, unsigned long int *fileSize);

#endif 
