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

// Alarm function handler
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}

int linkTx(LinkLayer connection) {
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = argv[1];

    /*if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }*/

    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);

    if (fd < 0)
    {
        perror(serialPortName);
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

    // Create string to send
    unsigned char SET[5] = {0};
    unsigned char F = 0x7E;
    unsigned char A = 0x03;
    unsigned char C = 0x03;
    unsigned char BCC1 = A ^ C;

    SET[0] = F;
    SET[1] = A;
    SET[2] = C;
    SET[3] = BCC1;
    SET[4] = F;

    // In non-canonical mode, '\n' does not end the writing.
    // Test this condition by placing a '\n' in the middle of the buffer.
    // The whole buffer must be sent even with the '\n'.
    // buf[5] = '\n';

    
    unsigned char UA_buffer[1] = {0};   
    unsigned char state = START; 

    // UA buffer that is sent as an answer by the receiver
    unsigned char UA_FLAG = 0x7E;
    unsigned char UA_A = 0x03;
    unsigned char UA_C = 0x07;
    unsigned char UA_BCC1 = UA_A ^ UA_C;

    // Set alarm function handler
    (void)signal(SIGALRM, alarmHandler);

    int result = -1;

    while (alarmCount < connection.nRetransmissions && LINKED == FALSE)
    {
        int bytes = write(fd, SET, 5);
        printf("%d bytes written\n", bytes);

        // Wait until all bytes have been written to the serial port
        sleep(1);
        if (alarmEnabled == FALSE)
        {
            alarm(connection.timeout); // Set alarm to be triggered in 3s
            alarmEnabled = TRUE;

            while (STOP == FALSE && alarmEnabled == TRUE) {
            
                int bytes = read(fd, UA_buffer, 1);
                // printf("Message received: 0x%02X \n Bytes read: %d\n", UA_buffer[0], bytes);

                // state machine
                switch(UA_buffer[0]) {
                    case 0x01:  //UA_A
                        if (state == UA_FLAG)
                            state = UA_A;
                        else 
                            state = START;
                        break;

                    case 0x07:  //UA_C
                        if (state == UA_A)
                            state = UA_C;
                        else 
                            state = START;
                        break;

                    case (0x03 ^ 0x07):  //UA_BCC1
                        if (state == UA_C)
                            state = UA_BCC1;
                        else
                            state = START;
                        break;

                    case 0x7E:  //UA_FLAG
                        if (state == UA_BCC1) {
                            LINKED = TRUE;
                            state = START;
                            result = 1;

                            printf("Successful reception\n");
                            alarm(0);   // alarm is disabled   

                            /*int bytes = write(fd, SET, 5);
                            printf("%d SET bytes written\n", bytes);*/
                        }
                        else
                            state = UA_FLAG;
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
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = argv[1];

    /*if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }*/

    // Open serial port device for reading and writing and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(serialPortName);
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

    unsigned char FLAG = 0x7E;
    unsigned char A = 0x03;
    unsigned char C = 0x03;
    unsigned char BCC1 = A ^ C;
   
    unsigned char START = 0xFF;
    
    // Loop for input
    unsigned char buf[1] = {0}; // +1: Save space for the final '\0' char
    unsigned char state = START;

    int result = -1;

    // STATE MACHINE
    while (LINKED == FALSE)
    {
        // Returns after 5 chars have been input
        int bytes = read(fd, buf, 1);
        //buf[bytes] = '\0'; // Set end of string to '\0', so we can printf
        
        printf("Message received: 0x%02X \n Bytes read:%d\n", buf[0], bytes);
        //printf("buf = 0x%02X\n", (unsigned int)(buf & 0xFF));

        switch (buf[0]) {
            case 0x03:  // can be A or C
              if (state == FLAG) {
                state = A;
              }
              else if (state == A) {
                state = C;
              }
              else {
                state = START;
              }
              break;
              
            case 0x00:  // BCC1
              if (state == C) {
                state = BCC1;
              }
              else {
                state = START;
              }
              break;
              
            case 0x7E:  // FLAG
              if (state == BCC1) {
                LINKED = TRUE;    // ends the main loop
                state = START;
                printf("Succesful reception of SET message");
                result = 1;

                // Sending the response (UA)
                unsigned char UA_A = 0x03;
                unsigned char UA_C = 0x07;
                BCC1 = UA_A ^ UA_C;
                unsigned char UA[5];
                UA[0] = FLAG;
                UA[1] = UA_A;
                UA[2] = UA_C;
                UA[3] = BCC1;
                UA[4] = FLAG;
                int bytes = write(fd, UA, 5);
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

    return result

    if (LINKED == TRUE) {       // I think this main if belongs to the llread function
      int STOP = FALSE;
      // unsigned char data[5];

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
        /*switch (buf[0]) {
          case 0x03:  // can be A or C
            if (state == FLAG) {
              state = A;
            }
            else if (state == A) {
              state = C;
            }
            else {
              // process data
            }
            break;
            
          case 0x00:  // BCC1
            if (state == C) {
              state = BCC1;
            }
            else {
              // process data
            }
            break;

          case 0x01:  // BCC2 -------- to check
            if (state == BCC1) {
              state = BCC2;
            }
            else {
              // process data
            }
            break;
            
          case 0x7E:  // FLAG
            if (state == BCC2) {
              STOP = TRUE;    // ends the loop
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
      }*/
    }
}