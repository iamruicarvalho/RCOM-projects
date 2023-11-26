// Application layer protocol implementation

#include "application_layer.h"
#include "auxiliar_funcs.h"
#include "link_layer.h"

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayerRole enumRole;
    if (strcmp(role, "tx") == 0) {
        enumRole = LlTx;
    } else if (strcmp(role, "rx") == 0) {
        enumRole = LlRx;
    }

    LinkLayer connectionParameters;
    strcpy(connectionParameters.serialPort,serialPort);
    connectionParameters.role = enumRole;
    connectionParameters.baudRate = baudRate;
    connectionParameters.nRetransmissions = nTries;
    connectionParameters.timeout = timeout;

    printf("\nEstablishing connection..\n");
    int openResult = llopen(connectionParameters);
    if (openResult < 0) {
        perror("\nError: failed to establish connection. Terminating program.\n");
        exit(-1);
    }
    else if (openResult == 1) {

        printf("\nConnection established\n\nTransferring file..\n");
        FILE* file;

        if (enumRole == LlTx) {

            file = fopen(filename,"rb");
            if (file == NULL) {
                perror("File not found\n");
                exit(-1);
            }

            int prev = ftell(file);
            fseek(file, 0L, SEEK_END);
            long int fileSize = ftell(file)-prev;   // file size: 10968
            fseek(file, prev, SEEK_SET);

            // signal the start of the transfer by sending a control packet
            // cField (values: 2 – startPacket; 3 – endPacket)
            unsigned int controlPacketSize;
            unsigned char *controlPacketStart = getControlPacket(2, filename, fileSize, &controlPacketSize);    // control packet size: 18
            int startingBytes = llwrite(controlPacketStart, controlPacketSize);

            if (startingBytes == -1) {
                printf("Error: could not send start packet\n");
                exit(-1);
            }

            unsigned char sequence = 0;
            unsigned char* content = getData(file, fileSize);
            long int bytesLeft = fileSize;

            while (bytesLeft >= 0) {

                int dataSize = bytesLeft > (long int) MAX_PAYLOAD_SIZE ? MAX_PAYLOAD_SIZE : bytesLeft;
                unsigned char* data = (unsigned char*) malloc(dataSize);
                memcpy(data, content, dataSize);
                int packetSize;
                unsigned char* packet = getDataPacket(sequence, data, dataSize, &packetSize);

                if(llwrite(packet, packetSize) == -1) {
                    printf("\nError during data packets transmission. Terminating program.\n");
                    exit(-1);
                }

                bytesLeft -= (long int) MAX_PAYLOAD_SIZE;
                content += dataSize;
                sequence = (sequence + 1) % 255;
            }

            // signal the end of the transfer by sending a control packet again
            unsigned char *controlPacketEnd = getControlPacket(3, filename, fileSize, &controlPacketSize);
            int endingBytes = llwrite(controlPacketEnd, controlPacketSize);

            if (endingBytes == -1) {
                printf("Error: could not send end packet\n");
                exit(-1);
            }

            int showStatistics = FALSE;
            int closeResult = llclose(showStatistics);
            fclose(file);

            if (closeResult == 1)
              printf("File transferred correctly and connection closed successfuly\n");

            else
              printf("An error occurred while closing the connection\n");
        }

        else {  // enumRole == LlRx

            unsigned char *packet = (unsigned char*)malloc(MAX_PAYLOAD_SIZE);
            int packetSize = -1;
            while ((packetSize = llread(packet)) < 0);

            unsigned long int rxFileSize = 0;
            parseControlPacket(packet, packetSize, &rxFileSize);
            FILE* newFile = fopen((char*) filename, "wb+");

            while (TRUE) {
                while ((packetSize = llread(packet)) < 0);
                if (packetSize == 0)
                    break;
                else if (packet[0] != 3) {      // if this is not the controlPacketEnd, we will process the control packet
                    unsigned char *buffer = (unsigned char*)malloc(packetSize);
                    parseDataPacket(packet, packetSize, buffer);
                    fwrite(buffer, sizeof(unsigned char), packetSize - 4, newFile);
                    free(buffer);
                }
                else continue;
            }

            fclose(newFile);
            printf("File transferred correctly and connection closed successfuly\n");
        }

    }
    else {
        printf("An error occurred in the linking process. Terminating the program\n");
        exit(-1);
    }
}
