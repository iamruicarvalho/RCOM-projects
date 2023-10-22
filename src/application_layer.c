// Application layer protocol implementation

#include <stdio.h>
#include <string.h>
#include "application_layer.h"
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

    LinkLayer connectionParameters = {serialPort, enumRole, baudRate, nTries, timeout};
    strcpy(connectionParameters.serialPort, serialPort);

    int openResult = llopen(connectionParameters);

    if (openResult == 1) {
        FILE* file;

        if (enumRole == LlTx) {

            file = fopen(filename,"rb");    
            if (file == NULL) {             
                perror("File not found\n"); 
                exit(-1);
            }   
            
            const unsigned char* buf;       
            fread(buf, size, 1, file);      

            int bytes = llwrite(buf, size);
            printf("%i bytes written", bytes);    // only for debugging

        } else {  // enumRole == LlRx

          unsigned char* packet;      // not sure where this should come from
          int bytes = llread(packet);
          printf("%i bytes read", bytes);    // only for debugging

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
        printf("An error occurred in the linking process. Terminating the
          program");
    }

}
