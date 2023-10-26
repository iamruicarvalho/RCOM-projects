// Application layer protocol implementation

#include "application_layer.h"
// #include "link_layer.h"
#include "auxiliar_funcs.h"

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayerRole enumRole;
    if (strcmp(role, "tx") == 0) {
        enumRole = LlTx;
    } else if (strcmp(role, "rx") == 0) {
        enumRole = LlRx;
    }

    char *auxSerialPort = NULL;
    strcpy(auxSerialPort, serialPort);

    LinkLayer connectionParameters = {{(*auxSerialPort), enumRole, baudRate, nTries, timeout}};

    int openResult = llopen(connectionParameters);

    if (openResult == 1) {
        FILE* file;

        if (enumRole == LlTx) {

            file = fopen(filename,"rb");
            size_t size = sizeof(file);
            if (file == NULL) {
                perror("File not found\n");
                exit(-1);
            }

            unsigned char* buf = NULL;
            fread(buf, size, 1, file);

            int bytes = llwrite(buf, size);
            printf("%i bytes written", bytes);    // only for debugging

        } else {  // enumRole == LlRx

          unsigned char *packet = (unsigned char*)malloc(MAX_PAYLOAD_SIZE);
          int packetSize = -1;
          while ((packetSize = llread(packet)) < 0);
          unsigned long int rxFileSize = 0;
          unsigned char* name = parseControlPacket(packet, packetSize, &rxFileSize);

          FILE* newFile = fopen((char *) name, "wb+");
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

        if (closeResult == 1) {
          printf("Connection closed successfuly");
        }
        else
          printf("An error occurred while closing the connection");

    } else {
        printf("An error occurred in the linking process. Terminating the program");
    }
}
