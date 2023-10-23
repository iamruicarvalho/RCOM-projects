// Link layer protocol implementation
// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

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

#define BUF_SIZE 256

volatile int STOP = FALSE;
volatile int LINKED = FALSE;
int alarmEnabled = FALSE;
int alarmCount = 0;
int timeout = 0;
int retransmissions = 0;
unsigned char tramaTx = 0;
unsigned char tramaRx = 1;
const char* serialPort;
unsigned char START = 0xFF;
unsigned char tramaTx = 0;
unsigned char tramaRx = 1;

////////////////////////////////////////////////
// LLOPEN
////////////////////#include <termios.h>////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    int result;
    timeout = connectionParameters.timeout;
    retransmissions = connectionParameters.nRetransmissions;
    serialPort = connectionParameters.serialPort;

    if (connectionParameters.role == LlTx) {
        result = linkTx(connectionParameters);
    }
    else {
        result = linkRx(connectionParameters);
    }

    return result;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    int fd = makeConnection(serialPort);

    // prepare I frame to send
    unsigned char* I_buf = (unsigned char*) malloc(bufSize + 6);
    I_buf[0] = FLAG;
    I_buf[1] = 0x03;
    I_buf[2] = C_N(tramaTx);
    I_buf[3] = I_buf[1] ^ I_buf[2];

    // data packet
    int currPosition = 4;
    for (int i = 0; i < bufSize; i++) {
        if (buf[i] == FLAG) {
            I_buf[currPosition++] = ESC;
            I_buf[currPosition++] = 0x5E;
        }
        else if (buf[i] == ESC) {
            I_buf[currPosition++] = ESC;
            I_buf[currPosition++] = 0x5D;
        }
        else
            I_buf[currPosition++] = buf[i];
    }

    // building BCC2: xor of all D's
    unsigned char BCC2 = buf[0];
    for (unsigned int i = 1 ; i < bufSize ; i++) 
        BCC2 ^= buf[i];

    I_buf[currPosition++] = BCC2;
    I_buf[currPosition++] = FLAG;
    int size_I_buf = currPosition;

    unsigned char data[5] = {0};
    unsigned char state = START;
    (void) signal(SIGALRM, alarmHandler);

    int accepted = 0;
    int rejected = 0;

    while (alarmCount < retransmissions && LINKED == FALSE && !accepted && !rejected)
    {
        int bytes = write(fd, I_buf, size_I_buf);
        printf("%d bytes written\n", bytes);

        // Wait until all bytes have been written to the serial port
        sleep(1);
        if (alarmEnabled == FALSE)
        {
            alarm(timeout); // Set alarm to be triggered in 3s
            alarmEnabled = TRUE;

            while (alarmEnabled == TRUE && STOP == FALSE) {
                int result = read(fd, data, 5);

                if (!result)
                    continue;

                else if (result == C_REJ(0) || result == C_REJ(1)) // I frame rejected. need to read again
                    rejected = 1;

                else if (result == C_RR(0) || result == C_RR(1)) { // I frame accepted
                    accepted = 1;
                    tramaTx = (tramaTx+1) % 2;    // need to check this later
                    alarmEnabled = FALSE;
                }
                else 
                    continue;
            }
            if (accepted)   // I frame sent correctly. we can get out of the while
                break;
            alarmCount++;
        }
    }
    if (accepted)
        return size_I_buf;
    else {
        llclose(fd);
        return -1;
    }
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    FILE* file;
    file = fopen(filename,"w");
    fwrite(packet, size, 1, file);

    // ------------------------------------------
    if (LINKED == TRUE) {       // I think this main if belongs to the llread function
      int STOP = FALSE;
      unsigned char data[5];

      while (STOP == FALSE) {
        int bytes = read(fd, buf, 1);
        unsigned char A = 0x03;
        unsigned char C = 0x00;
        unsigned char BCC1 = A ^ C;
        // unsigned char BCC2 = ;   xor of all d's
        switch (buf[0]) {               // need to check the state machine with juani
          case 0x03:  // can be A or C
            if (state == FLAG) {
              state = A;
            }
            else {
                // process data
            }
            break;

          case 0x00:
            if (state == A) {
              state = C;
            }
            else {
              // process data
            }
            break;

          case (0x03 ^ 0x00):  // BCC1
            if (state == C) {
              state = BCC1;
            }
            else {
              // process data
            }
            break;

        //case (xor of all d's):  // BCC2 -------- to check
            if (state == BCC1) {
              state = BCC2;
            }
            else {
              // process data
            }
            break;

          case 0x7E:  // FLAG
            if (state == BCC2) {
              LINKED = TRUE;    // ends the loop
              state = START;
            }
            else {
              state = FLAG;
            }
            break;
          default:
            if (state == BCC1) {
              // process data
            }
        }
      }
    } // ------------------------------------------------

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int fd)
{
    // UA buffer that is sent as an answer by the receiver
    unsigned char DISC_buf[1] = {0};

    unsigned char state = START;

    int result = -1;

    // Set alarm function handler
    (void) signal(SIGALRM, alarmHandler);

    while (alarmCount < retransmissions && LINKED == FALSE)
    {
        // send SET buffer
        int bytes = sendSupervisionFrame(fd, A_DISC, A_DISC);
        printf("%d bytes written\n", bytes);

        // Wait until all bytes have been written to the serial port
        sleep(1);
        if (alarmEnabled == FALSE)
        {
            alarm(timeout); // Set alarm to be triggered in 3s
            alarmEnabled = TRUE;

            while (STOP == FALSE && alarmEnabled == TRUE) {

                int bytes = read(fd, DISC_buf, 1);
                // printf("Message received: 0x%02X \n Bytes read: %d\n", UA_buffer[0], bytes);

                // state machine
                switch(DISC_buf[0]) {
                    case A_DISC:  // 0x03
                        if (state == FLAG)
                            state = A_DISC;
                        else
                            state = START;
                        break;

                    case C_DISC:  //0x0B
                        if (state == A_DISC)
                            state = C_DISC;
                        else
                            state = START;
                        break;

                    case (BCC1_DISC):
                        if (state == C_DISC)
                            state = BCC1_DISC;
                        else
                            state = START;
                        break;

                    case FLAG:
                        if (state == BCC1_DISC) {
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
    // send the UA buffer to the receiver
    sendSupervisionFrame(fd, 0x03, C_UA);

    struct termios oldtio;

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    sleep(1);
    return close(fd);
}

int sendSupervisionFrame(int fd, unsigned char A, unsigned char C) {
    unsigned char buf[5] = {FLAG, A, C, A ^ C, FLAG};
    return write(fd, buf, 5);
}
