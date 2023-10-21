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

#define FALSE 0
#define TRUE 1
#define BUF_SIZE 256

volatile int STOP = FALSE;
volatile int LINKED = FALSE;
int alarmEnabled = FALSE;
int alarmCount = 0;
unsigned char START = 0xFF;

////////////////////////////////////////////////
// LLOPEN
////////////////////#include <termios.h>////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    int result;

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
    unsigned char I0_buf[6] ={0};
    unsigned char F = 0x7E;
    unsigned char A = 0x03;
    unsigned char C = 0x00;
    unsigned char BCC1 = A ^ C;
    unsigned char BCC2 = ;     // xor of all d's

    I0_buf[0] = F;
    I0_buf[1] = A;
    I0_buf[2] = C;
    I0_buf[3] = BCC1;
    I0_buf[4] = BCC2;
    I0_buf[5] = F;

    unsigned char RR1_buffer[1] = {0};   
    unsigned char state = START; 

    // UA buffer that is sent as an answer by the receiver
    unsigned char RR1_FLAG = 0x7E;
    unsigned char RR1_A = 0x03;
    unsigned char RR1_C = 0x85;
    unsigned char REJ1_C = 0x81;
    unsigned char RR1_BCC1 = RR1_A ^ RR1_C;

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

    while (alarmCount < connection.nRetransmissions && LINKED == FALSE)
    {
        // send SET buffer
        int bytes = sendSupervisionFrame(fd, A_DISC, A_DISC);
        printf("%d bytes written\n", bytes);

        // Wait until all bytes have been written to the serial port
        sleep(1);
        if (alarmEnabled == FALSE)
        {
            alarm(connection.timeout); // Set alarm to be triggered in 3s
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

// Alarm function handler
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}

int sendSupervisionFrame(int fd, unsigned char A, unsigned char C) {
    unsigned char BCC1 = A ^ C;
    unsigned char buffer[5] = {FLAG, A, C, BCC1, FLAG};
    
    return write(fd, buffer, 5);
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
    newtio.c_cc[VMIN] = 1;  // Blocking read until 5 chars received

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