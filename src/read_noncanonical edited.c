// Read from serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 256

volatile int LINKED = FALSE;

int linkRx(int argc, char *argv[])
{
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = argv[1];

    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

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

    if (LINKED == TRUE) {
      int STOP = FALSE;
      unsigned char data[5];

      while (STOP == FALSE) {
        int bytes = read(fd, buf, 1);
        unsigned char BCC2 = A ^ C;   // -------- xor of all Dis
        switch (buf[0]) {
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
      }
    }

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return result;
}
