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

    int openResult = llopen(connectionParameters);
    if (openResult < 0) {
        perror("Error: failed to establish connection. Terminating program.\n");
        exit(-1);
    }
    else if (openResult == 1) {

        printf("\nConnection established\n");
        FILE* file;

        if (enumRole == LlTx) {

            file = fopen(filename,"rb");
            if (file == NULL) {
                perror("File not found\n");
                exit(-1);
            }

            int prev = ftell(file);
            fseek(file, 0L, SEEK_END);
            long int fileSize = ftell(file)-prev;
            fseek(file, prev, SEEK_SET);
            //printf("fileSize: %li\n", fileSize); // file size: 10968

            // signal the start of the transfer by sending the controlPacket
            // cField (values: 2 – startPacket; 3 – endPacket)
            unsigned int controlPacketSize;
            unsigned char *controlPacketStart = getControlPacket(2, filename, fileSize, &controlPacketSize);
            // printf("control packet size: %i\n", controlPacketSize);   // control packet size: 18
            int startingBytes = llwrite(controlPacketStart, controlPacketSize);

            if (startingBytes == -1) {
                printf("Error: could not send start packet\n");
                exit(-1);
            }
            else
                printf("Start control packet: %i bytes written\n", startingBytes);

            unsigned char sequence = 0;
            unsigned char* content = getData(file, fileSize);
            long int bytesLeft = fileSize;

            while (bytesLeft >= 0) {

                int dataSize = bytesLeft > (long int) MAX_PAYLOAD_SIZE ? MAX_PAYLOAD_SIZE : bytesLeft;
                unsigned char* data = (unsigned char*) malloc(dataSize);
                memcpy(data, content, dataSize);
                int packetSize;
                unsigned char* packet = getDataPacket(sequence, data, dataSize, &packetSize);
                printf("packetSize: %i\n", packetSize);
                if(llwrite(packet, packetSize) == -1) {
                    printf("Exit: error in data packets\n");
                    exit(-1);
                }

                bytesLeft -= (long int) MAX_PAYLOAD_SIZE;
                content += dataSize;
                sequence = (sequence + 1) % 255;
            }

            // signal the end of the transfer by sending the controlPacket again
            unsigned char *controlPacketEnd = getControlPacket(3, filename, fileSize, &controlPacketSize);
            int endingBytes = llwrite(controlPacketEnd, controlPacketSize);

            if (endingBytes == -1){
                printf("Error: could not send end packet\n");
                exit(-1);
            }
            printf("End control packet: %i bytes written\n", endingBytes);

        }
        else {  // enumRole == LlRx

            unsigned char *packet = (unsigned char*)malloc(MAX_PAYLOAD_SIZE);

            int packetSize = llread(packet);

            while (packetSize < 0);
            printf("packetSize read: %i\n", packetSize);

            unsigned long int rxFileSize = 0;
            unsigned char* name = parseControlPacket(packet, packetSize, &rxFileSize);
            FILE* newFile = fopen((char*) name, "wb+");

            while (1) {
                while ((packetSize = llread(packet)) < 0);
                // packetSize = llread(packet);
                printf("packetSize read: %i\n", packetSize);
                if (packetSize == 0)
                    break;
                else if (packet[0] != 3) {      // if this is not the controlPacketEnd, we will process the control packet
                    printf("Checkpoint\n");     // there is a problem with the end control packet
                    unsigned char *buffer = (unsigned char*)malloc(packetSize);
                    parseDataPacket(packet, packetSize, buffer);
                    fwrite(buffer, sizeof(unsigned char), packetSize - 4, newFile);
                    free(buffer);
                }
                else continue;
            }

            fclose(newFile);
        }

        int showStatistics = FALSE;
        int closeResult = llclose(showStatistics);
        fclose(file);

        if (closeResult == 1)
          printf("Connection closed successfuly\n");

        else
          printf("An error occurred while closing the connection\n");

    }
    else {
        printf("An error occurred in the linking process. Terminating the program\n");
        exit(-1);
    }
}
