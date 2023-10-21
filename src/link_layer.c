// Link layer protocol implementation

#include <termios.h>
#include "link_layer.h"
#include "auxiliar_funcs.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

////////////////////////////////////////////////
// LLOPEN
////////////////////#include <termios.h>////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    int result;

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
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    
    struct termios oldtio;

    // TODO 
    
    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    sleep(1);
    close(fd);

    return 1;
}
