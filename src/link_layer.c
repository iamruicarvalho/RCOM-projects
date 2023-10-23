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
    STOP = FALSE;
    alarmCount = 0;

    while (alarmCount < retransmissions && !accepted && !rejected)
    {
        int bytes = write(fd, I_buf, size_I_buf);
        printf("%d bytes written\n", bytes);

        // Wait until all bytes have been written to the serial port
        sleep(1);
        if (alarmEnabled == FALSE)
        {
            alarm(timeout); // Set alarm to be triggered in 3s
            alarmEnabled = TRUE;

            while (alarmEnabled == TRUE) {
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
        }
    }

    free(I_buf);
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
    LinkLayerState state = START_TX;
    unsigned char buf, cField;
    int i = 0;

    while (state != STOP_R) {  
        
        switch (state) {

            case START_TX:
                if (buf == FLAG) 
                    state = FLAG_RCV;
                break;

            case FLAG_RCV:
                if (buf == A_ER) 
                    state = A_RCV;
                else if (buf != FLAG)   
                    state = START_TX;
                break;

            case A_RCV:
                if (buf == C_N(0) || buf == C_N(1)) {
                    state = C_RCV;
                    cField = buf;   
                }
                else if (buf == FLAG) 
                    state = FLAG_RCV;
                else if (buf == C_DISC) {
                    sendSupervisionFrame(fd, A_RE, C_DISC);
                    return 0;
                }
                else 
                    state = START;
                break;

            case C_RCV:
                if (buf == (A_ER ^ cField)) 
                    state = READING_DATA;
                else if (buf == FLAG) 
                    state = FLAG_RCV;
                else 
                    state = START;
                break;

            case READING_DATA:
                if (buf == ESC) 
                    state = DATA_FOUND_ESC;
                else if (buf == FLAG) {
                    unsigned char bcc2 = packet[i-1];
                    i--;
                    packet[i] = '\0';   // check if it is necessary
                    unsigned char acc = packet[0];

                    for (unsigned int j = 1; j < i; j++)
                        acc ^= packet[j];

                    if (bcc2 == acc) {
                        state = STOP_R;
                        sendSupervisionFrame(fd, A_RE, C_RR(tramaRx));
                        tramaRx = (tramaRx + 1) % 2;
                        return i;   // amount of bytes read
                    }
                    else {
                        printf("An error ocurred, sending RREJ\n");
                        sendSupervisionFrame(fd, A_RE, C_REJ(tramaRx));
                        return -1;
                    };

                }
                else {
                    packet[i++] = buf;
                }
                break;

            case DATA_FOUND_ESC:
                state = READING_DATA;
                if (buf == ESC || buf == FLAG) 
                    packet[i++] = buf;
                else {
                    packet[i++] = ESC;
                    packet[i++] = buf;
                }
                break;
            default: 
                break;
        }
        
    }
    return -1;
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

