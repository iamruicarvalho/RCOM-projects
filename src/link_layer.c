// Link layer protocol implementation
// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

#include "auxiliar_funcs.h"
#include "link_layer.h"
#include "application_layer.h"

volatile int STOP = FALSE;
int alarmEnabled = FALSE;
int alarmCount = 0;
int timeout;
int retransmissions = 0;
unsigned char tramaTx = 0;
unsigned char tramaRx = 1;
unsigned char START = 0xFF;
int fd;

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define BUF_SIZE 256


////////////////////////////////////////////////
// LLOPEN
////////////////////#include <termios.h>////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    int result;

    fd = makeConnection(connectionParameters.serialPort);
    retransmissions = connectionParameters.nRetransmissions;
    timeout = connectionParameters.timeout;

    if (fd < 0)
      return -1;

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
    // // prepare I frame to send
    // unsigned char* I_buf = (unsigned char*) malloc(bufSize + 6);
    // I_buf[0] = FLAG;
    // I_buf[1] = 0x03;
    // I_buf[2] = C_N(tramaTx);
    // I_buf[3] = I_buf[1] ^ I_buf[2];
    //
    // // data packet
    // int currPosition = 4;
    // for (int i = 0; i < bufSize; i++) {
    //     if (buf[i] == FLAG) {
    //         I_buf[currPosition++] = ESC;
    //         I_buf[currPosition++] = 0x5E;
    //     }
    //     else if (buf[i] == ESC) {
    //         I_buf[currPosition++] = ESC;
    //         I_buf[currPosition++] = 0x5D;
    //     }
    //     else
    //         I_buf[currPosition++] = buf[i];
    // }
    //
    // // building BCC2: xor of all D's
    // unsigned char BCC2 = buf[0];
    // for (unsigned int i = 1 ; i < bufSize ; i++)
    //     BCC2 ^= buf[i];
    //
    // I_buf[currPosition++] = BCC2;
    // I_buf[currPosition++] = FLAG;
    // int size_I_buf = currPosition;

    int size_I_buf = 6+bufSize;
    unsigned char *I_buf = (unsigned char *) malloc(size_I_buf);
    I_buf[0] = FLAG;
    I_buf[1] = A_ER;
    I_buf[2] = C_N(tramaTx);
    I_buf[3] = I_buf[1] ^ I_buf[2];
    memcpy(I_buf+4,buf, bufSize);
    unsigned char BCC2 = buf[0];
    for (unsigned int i = 1 ; i < bufSize ; i++) BCC2 ^= buf[i];

    int j = 4;
    for (unsigned int i = 0 ; i < bufSize ; i++) {
        if(buf[i] == FLAG || buf[i] == ESC) {
            I_buf = realloc(I_buf,++size_I_buf);
            I_buf[j++] = ESC;
        }
        I_buf[j++] = buf[i];
    }
    I_buf[j++] = BCC2;
    I_buf[j++] = FLAG;

    (void) signal(SIGALRM, alarmHandler);

    int currentTransmition = 0;
    int accepted;
    int rejected;
    alarmCount = 0;

    while (currentTransmition < retransmissions) {
      alarmEnabled = FALSE;
      alarm(timeout);
      rejected = 0;
      accepted = 0;

      while (alarmEnabled == FALSE && !rejected && !accepted) {
        int bytes = write(fd, I_buf, size_I_buf);
        printf("llwrite sent %d bytes\n", bytes);

        // Wait until all bytes have been written to the serial port
        sleep(1);

        unsigned char result = readControlFrame();
        //printf("after readControlFrame: \n");

        if (!result)
            continue;

        else if (result == C_REJ(0) || result == C_REJ(1)) // I frame rejected. need to read again
            rejected = TRUE;

        else if (result == C_RR(0) || result == C_RR(1)) { // I frame accepted
            accepted = TRUE;
            tramaTx = (tramaTx+1) % 2;    // need to check this later
            //alarmEnabled = FALSE;
        }
        else
            continue;
      }
      printf("outside of main llwrite while\n");
      if (accepted)   // I frame sent correctly. we can get out of the while
          break;
      currentTransmition++;
    }

    free(I_buf);

    if (accepted)
        return size_I_buf;
    else {
        llclose(FALSE);
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
        //int bytes = read(fd, &buf, 1);
        //printf("llread read %i bytes\n", bytes);
        if (read(fd, &buf, 1) > 0) {
          printf("Message received: 0x%02X \n", buf);
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
                      sendSupervisionFrame(A_RE, C_DISC);
                      //printf("Sent supervision frame\n");
                      return 0;
                  }
                  else
                      state = START_TX;
                  break;

              case C_RCV:
                  if (buf == (A_ER ^ cField))
                      state = READING_DATA;
                  else if (buf == FLAG)
                      state = FLAG_RCV;
                  else
                      state = START_TX;
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
                          sendSupervisionFrame(A_RE, C_RR(tramaRx));
                          tramaRx = (tramaRx + 1) % 2;
                          return i;   // amount of bytes read
                      }
                      else {
                          printf("An error ocurred, sending RREJ\n");
                          sendSupervisionFrame(A_RE, C_REJ(tramaRx));
                          return -1;
                      };

                  }
                    else
                        packet[i++] = buf;
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

    }
    return -1;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    unsigned char byte;
    LinkLayerState state = START_TX;
    // Set alarm function handler
    (void) signal(SIGALRM, alarmHandler);

    while (alarmCount < retransmissions && state != STOP_R)
    {
        // send DISC buffer
        sendSupervisionFrame(A_DISC, C_DISC);
        alarm(timeout);
        alarmEnabled = FALSE;
        //printf("%d bytes written\n", bytes);

        // Wait until all bytes have been written to the serial port
        sleep(1);
        // if (alarmEnabled == FALSE)
        // {
        //     alarm(timeout); // Set alarm to be triggered in 3s
        //     alarmEnabled = TRUE;

            while (alarmEnabled == FALSE && state != STOP_R) {

                if (read(fd, &byte, 1)) {
                  //printf("Message received: 0x%02X \n Bytes read: %d\n", DISC_buf, bytes);

                  switch (state) {
                    case START_TX:
                        if (byte == FLAG) state = FLAG_RCV;
                        break;
                    case FLAG_RCV:
                        if (byte == A_RE) state = A_RCV;
                        else if (byte != FLAG) state = START_TX;
                        break;
                    case A_RCV:
                        if (byte == C_DISC) state = C_RCV;
                        else if (byte == FLAG) state = FLAG_RCV;
                        else state = START_TX;
                        break;
                    case C_RCV:
                        if (byte == (A_RE ^ C_DISC)) state = BCC1_OK;
                        else if (byte == FLAG) state = FLAG_RCV;
                        else state = START_TX;
                        break;
                    case BCC1_OK:
                        if (byte == FLAG) state = STOP_R;
                        else state = START_TX;
                        break;
                    default:
                        break;
                      }
                }
            }
            alarmCount++;
      }

      if (state != STOP_R) return -1;
      // send the UA buffer to the receiver
      sendSupervisionFrame(0x03, C_UA);
      sleep(1);

      close(fd);
      return 1;
}
