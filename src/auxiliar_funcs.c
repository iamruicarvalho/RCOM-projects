#include "auxiliar_funcs.h"
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

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1
#define BUF_SIZE 256

volatile int STOP = FALSE;
volatile int LINKED = FALSE;
int alarmEnabled = FALSE;
int alarmCount = 0;
unsigned char START = 0xFF;

int linkTx(LinkLayer connection) {

    // open serial port
    fd = makeConnection(connection.serialPort);
    if (fd < 0)
      return -1;

    // In non-canonical mode, '\n' does not end the writing.
    // Test this condition by placing a '\n' in the middle of the buffer.
    // The whole buffer must be sent even with the '\n'.
    // buf[5] = '\n';

    // UA buffer that is sent as an answer by the receiver
    unsigned char UA_buffer[1] = {0};

    unsigned char state = START;

    int result = -1;

    // Set alarm function handler
    (void) signal(SIGALRM, alarmHandler);

    while (alarmCount < connection.nRetransmissions && LINKED == FALSE)
    {
        // send SET buffer
        int bytes = sendSupervisionFrame(fd, A_SET, C_SET);
        printf("%d bytes written\n", bytes);

        // Wait until all bytes have been written to the serial port
        sleep(1);
        if (alarmEnabled == FALSE)
        {
            alarm(connection.timeout); // Set alarm to be triggered in 3s
            alarmEnabled = TRUE;

            while (STOP == FALSE && alarmEnabled == TRUE) {

                read(fd, UA_buffer, 1);
                // printf("Message received: 0x%02X \n Bytes read: %d\n", UA_buffer[0], bytes);

                // state machine
                switch(UA_buffer[0]) {
                    case A_UA:  // 0x01
                        if (state == FLAG)
                            state = A_UA;
                        else
                            state = START;
                        break;

                    case C_UA:  //0x07
                        if (state == A_UA)
                            state = C_UA;
                        else
                            state = START;
                        break;

                    case (BCC1_UA):  //0x01 ^ 0x07
                        if (state == C_UA)
                            state = BCC1_UA;
                        else
                            state = START;
                        break;

                    case FLAG:
                        if (state == BCC1_UA) {
                            LINKED = TRUE;
                            state = START;
                            result = 1;

                            printf("Successful reception\n");
                            alarm(0);   // alarm is disabled

                            /*int bytes = write(fd, SET, 5);
                            printf("%d SET bytes written\n", bytes);*/
                        }
                        else
                            state = FLAG;
                        break;
                    default:
                        state = START;
                }
            }
        }
    }
    return result;
}

// ----------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------

int linkRx(LinkLayer connection) {

    // open serial port
    fd = makeConnection(connection.serialPort);
    if (fd < 0)
      return -1;

    // Loop for input
    unsigned char buf[1] = {0}; // +1: Save space for the final '\0' char
    unsigned char state = START;
    int result = -1;

    // STATE MACHINE
    while (STOP == FALSE)
    {
        // Returns after 1 char has been input
        int bytes = read(fd, buf, 1);

        printf("Message received: 0x%02X \n Bytes read:%d\n", buf[0], bytes);   // only for debugging
        //printf("buf = 0x%02X\n", (unsigned int)(buf & 0xFF));

        switch (buf[0]) {
            case 0x03:  // can be A_SET or C_SET
              if (state == FLAG) {
                state = A_SET;
              }
              else if (state == A_SET) {
                state = C_SET;
              }
              else {
                state = START;
              }
              break;

            case (BCC1_SET):
              if (state == C_SET) {
                state = BCC1_SET;
              }
              else {
                state = START;
              }
              break;

            case FLAG:
              if (state == BCC1_SET) {
                STOP = TRUE;
                state = START;
                printf("Succesful reception of SET message");
                result = 1;

                // Sending the response (UA)
                int bytes = sendSupervisionFrame(fd, A_UA, C_UA);
                printf("\n%d UA bytes written\n", bytes);
              }
              else {
                state = FLAG;
              }
              break;
            default:
              state = START;
        }
    }

    return result;
}

// ----------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------

// Alarm function handler
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}

int sendSupervisionFrame(int fd, unsigned char A, unsigned char C) {
    unsigned char buf[5] = {FLAG, A, C, A ^ C, FLAG};
    return write(fd, buf, 5);
}

int makeConnection(const char* serialPort) {
    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPort, O_RDWR | O_NOCTTY);

    if (fd < 0)
    {
        perror(serialPort);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 1;  // Blocking read until 1 char received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    return fd;
}

unsigned char* parseControlPacket(unsigned char* packet, int size, unsigned long int *fileSize) {

    // File Size
    unsigned char fileSizeNBytes = packet[2];
    unsigned char fileSizeAux[fileSizeNBytes];
    memcpy(fileSizeAux, packet+3, fileSizeNBytes);
    for(unsigned int i = 0; i < fileSizeNBytes; i++)
        *fileSize |= (fileSizeAux[fileSizeNBytes-i-1] << (8*i));

    // File Name
    unsigned char fileNameNBytes = packet[3+fileSizeNBytes+1];
    unsigned char *name = (unsigned char*)malloc(fileNameNBytes);
    memcpy(name, packet+3+fileSizeNBytes+2, fileNameNBytes);
    return name;
}

void parseDataPacket(const unsigned char* packet, const unsigned int packetSize, unsigned char* buffer) {
    memcpy(buffer,packet+4,packetSize-4);
    buffer += packetSize+4;
}
