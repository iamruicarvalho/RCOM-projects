#include "link_layer.h"
#include "application_layer.h"
#include "auxiliar_funcs.h"

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1
#define BUF_SIZE 256

extern volatile int STOP;
extern int alarmEnabled;
extern int alarmCount;
extern int timeout;
extern int retransmissions;
extern unsigned char tramaTx;
extern unsigned char tramaRx;
extern unsigned char START;
extern int fd;

int linkTx(LinkLayer connection) {

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

    while (alarmCount < connection.nRetransmissions && STOP == FALSE)
    {
        // send SET buffer
        sendSupervisionFrame(A_SET, C_SET);

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
                            STOP = TRUE;
                            state = START;
                            result = 1;

                            // printf("Successful reception\n");
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

    unsigned char buf;
    unsigned char state = START;
    int result = -1;

    // STATE MACHINE
    while (STOP == FALSE)
    {
        // Returns after 1 char has been input
        read(fd, &buf, 1);

        //printf("Message received: 0x%02X \n Bytes read:%d\n", buf, bytes);   // only for debugging
        //printf("buf = 0x%02X\n", (unsigned int)(buf & 0xFF));

        switch (buf) {
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
                //printf("Succesful reception of SET message");
                result = 1;

                // Sending the response (UA)
                sendSupervisionFrame(A_UA, C_UA);
                //printf("\n%d UA bytes written\n", bytes);
                sleep(1);
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

int sendSupervisionFrame(unsigned char A, unsigned char C) {
    unsigned char buf[5] = {FLAG, A, C, A ^ C, FLAG};
    return write(fd, buf, 5);
}

int makeConnection(const char* serialPort) {
    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.

    fd = open(serialPort, O_RDWR | O_NOCTTY);

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

    //printf("New termios structure set\n");

    return fd;
}

unsigned char* getControlPacket(const unsigned int c, const char* filename, long int length, unsigned int* size) {

    const int L1 = (int) ceil(log2f((float)length)/8.0);
    const int L2 = strlen(filename);
    *size = 1 + 2 + L1 + 2 + L2;
    unsigned char *controlPacket = (unsigned char*)malloc(*size);

    unsigned int i = 0;
    controlPacket[i++] = c;       // control field: 2 - start, 3 - end
    controlPacket[i++] = 0;       // T: 0 - file size
    controlPacket[i++] = L1;      // L

    for (unsigned char j = 0 ; j < L1 ; j++) {      // V
        controlPacket[2 + L1 - j] = length & 0xFF;
        length >>= 8;
    }
    i += L1;
    controlPacket[i++] = 1;
    controlPacket[i++] = L2;
    memcpy(controlPacket + i, filename, L2);

    return controlPacket;
}

unsigned char* getDataPacket(unsigned char sequence, unsigned char *data, int dataSize, int *packetSize) {
    *packetSize = 1 + 1 + 2 + dataSize;
    unsigned char* packet = (unsigned char*)malloc(*packetSize);

    packet[0] = 1;
    packet[1] = sequence;
    packet[2] = dataSize >> 8 & 0xFF;
    packet[3] = dataSize & 0xFF;
    memcpy(packet+4, data, dataSize);

    return packet;
}

unsigned char* parseControlPacket(unsigned char* packet, int size, unsigned long int *fileSize) {

    // File Size
    unsigned char fileSizeNBytes = packet[2];
    unsigned char fileSizeAux[fileSizeNBytes];
    memcpy(fileSizeAux, packet + 3, fileSizeNBytes);
    for (unsigned int i = 0; i < fileSizeNBytes; i++)
        *fileSize |= (fileSizeAux[fileSizeNBytes - i - 1] << (8 * i));

    // File Name
    unsigned char fileNameNBytes = packet[3 + fileSizeNBytes + 1];
    unsigned char *name = (unsigned char*)malloc(fileNameNBytes);
    memcpy(name, packet + 3 + fileSizeNBytes + 2, fileNameNBytes);
    return name;
}

void parseDataPacket(const unsigned char* packet, const unsigned int packetSize, unsigned char* buffer) {
    memcpy(buffer, packet + 4, packetSize - 4);
    buffer += packetSize + 4;
}

unsigned char * getData(FILE* fd, long int fileLength) {
    unsigned char* content = (unsigned char*)malloc(sizeof(unsigned char) * fileLength);
    fread(content, sizeof(unsigned char), fileLength, fd);
    return content;
}

unsigned char readControlFrame() {

    unsigned char byte, cField = 0;
    LinkLayerState state = START_TX;

    while (state != STOP_R && alarmCount < retransmissions) {
        if (read(fd, &byte, 1) > 0 || 1) {
            switch (state) {
                case START_TX:
                    if (byte == FLAG) state = FLAG_RCV;
                    break;
                case FLAG_RCV:
                    if (byte == A_RE) state = A_RCV;
                    else if (byte != FLAG) state = START_TX;
                    break;
                case A_RCV:
                    if (byte == C_RR(0) || byte == C_RR(1) || byte == C_REJ(0) || byte == C_REJ(1) || byte == C_DISC){
                        state = C_RCV;
                        cField = byte;
                    }
                    else if (byte == FLAG) state = FLAG_RCV;
                    else state = START_TX;
                    break;
                case C_RCV:
                    if (byte == (A_RE ^ cField)) state = BCC1_OK;
                    else if (byte == FLAG) state = FLAG_RCV;
                    else state = START_TX;
                    break;
                case BCC1_OK:
                    if (byte == FLAG){
                        state = STOP_R;
                    }
                    else state = START_TX;
                    break;
                default:
                    break;
            }
        }
    }
    return cField;
}
