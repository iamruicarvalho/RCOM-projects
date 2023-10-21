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
    
    int result = llopen(connectionParameters);

    if (result == 1) {

        if (enumRole == LlTx) {
            FILE* file;
            file = fopen(filename,"rb");

            int size = sizeof(file);
            const unsigned char* buf = malloc(size);
            fread(buf, size, 1, file);

            int bytes = llwrite(buf, size);       // to check
            printf("%i bytes written", bytes);

        } else {

            // TODO

        } 
        

    } else {
        printf("An error occurred in the linking process");
    }

}
