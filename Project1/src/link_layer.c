// Link layer protocol implementation

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
////////////////////////////////////////////////
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
    // prepare I frame to send
    int size_I_buf = bufSize + 6;
    unsigned char* I_buf = (unsigned char *) malloc(size_I_buf);
    I_buf[0] = FLAG;
    I_buf[1] = A_ER;
    I_buf[2] = C_N(tramaTx);
    I_buf[3] = I_buf[1] ^ I_buf[2];
    memcpy(I_buf + 4, buf, bufSize);

    // building BCC2: xor of all D's
    unsigned char BCC2 = buf[0];
    for (unsigned int i = 1 ; i < bufSize ; i++) BCC2 ^= buf[i];

    // data packet
    int j = 4;
    for (unsigned int i = 0 ; i < bufSize ; i++) {
        if (buf[i] == FLAG || buf[i] == ESC) {
            I_buf = realloc(I_buf,++size_I_buf);
            I_buf[j++] = ESC;
        }
        I_buf[j++] = buf[i];
    }
    I_buf[j++] = BCC2;
    I_buf[j++] = FLAG;

    int accepted;
    int rejected;
    alarmCount = 0;

    while (alarmCount < retransmissions) {
      alarm(timeout);
      alarmEnabled = TRUE;
      rejected = 0;
      accepted = 0;

      while (alarmEnabled == TRUE && !rejected && !accepted) {
        write(fd, I_buf, size_I_buf);

        // Wait until all bytes have been written to the serial port
        sleep(1);
        unsigned char result = readControlFrame();

        if (!result)
            continue;

        else if (result == C_REJ(0) || result == C_REJ(1)) // I frame rejected, needs to read again
            rejected = TRUE;

        else if (result == C_RR(0) || result == C_RR(1)) { // I frame accepted
            accepted = TRUE;
            tramaTx = (tramaTx+1) % 2;   
        }
        else
            continue;
      }

      if (accepted)   // I frame sent correctly
          break;
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
        if (read(fd, &buf, 1) > 0) {
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
                      packet[i] = '\0';  
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
    
    (void) signal(SIGALRM, alarmHandler);

    while (alarmCount < retransmissions && state != STOP_R)
    {
        sendSupervisionFrame(A_DISC, C_DISC);
        alarm(timeout);
        alarmEnabled = FALSE;

        sleep(1);

        while (alarmEnabled == FALSE && state != STOP_R) {

            if (read(fd, &byte, 1)) {
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
