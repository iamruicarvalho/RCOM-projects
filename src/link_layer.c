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
unsigned char START = 0xFF;
unsigned char tramaTx = 0;
unsigned char tramaRx = 1;

////////////////////////////////////////////////
// LLOPEN
////////////////////#include <termios.h>////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    int result;
    int timeout = connectionParameters.timeout;
    int retransmissions = connectionParameters.nRetransmissions;

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
    unsigned char* I_buf = (unsigned char*) malloc(bufSize + 6);
    I_buf[0] = FLAG;
    I_buf[1] = 0x03;
    I_buf[2] = C_N(tramaTx);
    I_buf[3] = I_buf[1] ^ I_buf[2];

    // copy the content of buf to the next position of I_buf
    unsigned char* currPosition = I_buf + 4;
    memcpy(currPosition, buf, bufSize);    

    // building BCC2: xor of all D's
    unsigned char BCC2 = buf[0];
    for (unsigned int i = 1 ; i < bufSize ; i++) 
        BCC2 ^= buf[i];

    unsigned char nextPosition = currPosition++;
    unsigned char finalPosition = nextPosition++;
    I_buf[nextPosition] = BCC2;
    I_buf[finalPosition] = FLAG;


    

    /*unsigned char RR1_buffer[1] = {0};
    unsigned char state = START;

    // UA buffer that is sent as an answer by the receiver
    unsigned char RR1_FLAG = 0x7E;
    unsigned char RR1_A = 0x03;
    unsigned char RR1_C = 0x85;
    unsigned char REJ1_C = 0x81;
    unsigned char RR1_BCC1 = RR1_A ^ RR1_C;*/
    //-------------------------------------------------------



    // send I(Ns=0) (see how the RR and REJ works)
    while (alarmCount < connection.nRetransmissions && LINKED == FALSE)
    {
        int bytes = write(fd, buf, 6);
        printf("%d bytes written\n", bytes);

        // Wait until all bytes have been written to the serial port
        sleep(1);
        if (alarmEnabled == FALSE)
        {
            alarm(connection.timeout); // Set alarm to be triggered in 3s
            alarmEnabled = TRUE;

            while (STOP == FALSE && alarmEnabled == TRUE) {

                int bytes = read(fd, RR1_buffer, 1);
                // printf("Message received: 0x%02X \n Bytes read: %d\n", UA_buffer[0], bytes);

                // state machine
                switch(RR1_buffer[0]) {
                    case 0x03:  //RR1_A
                        if (state == RR1_FLAG)
                            state = RR1_A;
                        else
                            state = START;
                        break;

                    case 0x85:  //RR1_C
                        if (state == RR1_A)
                            state = RR1_C;
                        else
                            state = START;
                        break;

                    case (0x03 ^ 0x85):  //RR1_BCC1
                        if (state == RR1_C)
                            state = RR1_BCC1;
                        else
                            state = START;
                        break;

                    case 0x7E:  //RR1_FLAG
                        if (state == RR1_BCC1) {
                            LINKED = TRUE;
                            state = START;
                            result = 1;

                            printf("Successful reception\n");
                            alarm(0);   // alarm is disabled

                            /*int bytes = write(fd, SET, 5);
                            printf("%d SET bytes written\n", bytes);*/
                        }
                        else
                            state = RR1_FLAG;
                        break;
                    default:
                        state = START;
                }
            }
        }
    }
    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
  file = fopen(filename,"w");
  fwrite(packet, size, 1, file);

  // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int fd)
{
    // UA buffer that is sent as an answer by the receiver
    unsigned char DISC_buf[1] = {0};

    /*unsigned char UA_FLAG = 0x7E;
    unsigned char UA_A = 0x03;
    unsigned char UA_C = 0x07;
    unsigned char UA_BCC1 = UA_A ^ UA_C;*/
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
