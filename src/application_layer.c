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
    printf("Connection established\n");

    if (openResult == 1) {
        FILE* file;

        if (enumRole == LlTx) {

            file = fopen(filename,"rb");
            size_t fileSize = sizeof(file);
            if (file == NULL) {
                perror("File not found\n");
                exit(-1);
            }

            /*unsigned char* buf = (unsigned char*)malloc(fileSize);
            fread(buf, fileSize, 1, file);*/

            // cField (values: 2 – start; 3 – end)
            unsigned int cpSize;
            unsigned char *controlPacketStart = getControlPacket(2, filename, fileSize, &cpSize);

            int startingBytes = llwrite(controlPacketStart, cpSize);
            printf("Start control packet: %i bytes written", startingBytes);    // only for debugging

            // unsigned char sequence = 0;
            // unsigned char* content = getData(file, fileSize);
            // long int bytesLeft = fileSize;

            // while (bytesLeft >= 0) { 

            //     int dataSize = bytesLeft > (long int) MAX_PAYLOAD_SIZE ? MAX_PAYLOAD_SIZE : bytesLeft;
            //     unsigned char* data = (unsigned char*) malloc(dataSize);
            //     memcpy(data, content, dataSize);
            //     int packetSize;
            //     unsigned char* packet = getDataPacket(sequence, data, dataSize, &packetSize);
                
            //     if(llwrite(fd, packet, packetSize) == -1) {
            //         printf("Exit: error in data packets\n");
            //         exit(-1);
            //     }
                
            //     bytesLeft -= (long int) MAX_PAYLOAD_SIZE; 
            //     content += dataSize; 
            //     sequence = (sequence + 1) % 255;   
            // }

            // same thing as before but this time is to signal the end of the file transfer
            // cField (values: 2 – start; 3 – end)
            unsigned char *controlPacketEnd = getControlPacket(3, filename, fileSize, &cpSize);
            int endingBytes = llwrite(controlPacketEnd, cpSize); 
            printf("End control packet: %i bytes written", endingBytes);
            
            llclose(fd);
            // free(buf);
        }
        else {  // enumRole == LlRx

            unsigned char *packet = (unsigned char*)malloc(MAX_PAYLOAD_SIZE);
            int packetSize = -1;
            while ((packetSize = llread(packet)) < 0);
            printf("Checkpoint \n");
            unsigned long int rxFileSize = 0;
            unsigned char* name = parseControlPacket(packet, packetSize, &rxFileSize);

            FILE* newFile = fopen((char*) name, "wb+");
            while (1) {
                while ((packetSize = llread(packet)) < 0);
                if(packetSize == 0) break;
                else if(packet[0] != 3){
                    unsigned char *buffer = (unsigned char*)malloc(packetSize);
                    parseDataPacket(packet, packetSize, buffer);
                    fwrite(buffer, sizeof(unsigned char), packetSize-4, newFile);
                    free(buffer);
                }
                else continue;
            }
            printf("%i bytes read", packetSize);    // only for debugging
        }

        int showStatistics = FALSE;
        int closeResult = llclose(showStatistics);
        fclose(file);

        if (closeResult == 1) 
          printf("Connection closed successfuly");
        
        else
          printf("An error occurred while closing the connection");

    } else 
        printf("An error occurred in the linking process. Terminating the program");
    
}
