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
    int fd = makeConnection(connection.serialPort);
    if (fd < 0) 
      return -1;

  
    // SET buffer to send
    /*unsigned char SET[5] = {0};
    unsigned char F = 0x7E;
    unsigned char A = 0x03;
    unsigned char C = 0x03;
    unsigned char BCC1 = A ^ C;

    SET[0] = F;
    SET[1] = A;
    SET[2] = C;
    SET[3] = BCC1;
    SET[4] = F;*/

    // In non-canonical mode, '\n' does not end the writing.
    // Test this condition by placing a '\n' in the middle of the buffer.
    // The whole buffer must be sent even with the '\n'.
    // buf[5] = '\n';

    
    // UA buffer that is sent as an answer by the receiver
    unsigned char UA_buffer[1] = {0};   

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
        int bytes = sendSupervisionFrame(fd, A_SET, C_SET);
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
    int fd = makeConnection(connection.serialPort);
    if (fd < 0) 
      return -1;


    /*unsigned char FLAG = 0x7E;
    unsigned char A = 0x03;
    unsigned char C = 0x03;
    unsigned char BCC1 = A ^ C;*/
       
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
                LINKED = TRUE;    // ends the main loop
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