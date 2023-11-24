/**
 *  * @file fuzzer.c
 * @author Tom Gordon
 * @brief Fuzzing application to test the robustness of uP to handle as many rediculous serial inputs
 * as can be imagined.
 * 
 * @copyright Copyright (c) 2023, Gordon Innovations
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include "comms.h"  // serial communications

#ifdef __linux__
char const * const devstr = "/dev/pts/4";
#else
char const * const devstr = "COM7";
#endif // __linux__

#define kMaxCommands 8
#define kMaxParams 4
#define kMaxString 16

// Macro to generate a random printable character {' '..'~'}
#define RANDOM_NONSPACE_PRINTABLE ((char)(rand() * ((int)'~' - (int)' ') / RAND_MAX + (int)' ' + 1))

// Macro to generate a random, 7-bit, non-zero ASCII character {1..127}
#define RANDOM_NONZERO_ASCII ((char)(rand() * 126 / RAND_MAX + 1))

// Macro to generate a random, non-zero, 8-bit binary value {1..255}
#define RANDOM_NONZERO_CHAR ((unsigned char)(rand() * 254 / RAND_MAX + 1))

// Macro to generate a random numerical character {'0'..'9'}
#define RANDOM_NUMER_CHAR ((char)(rand() * 9 / RAND_MAX + '0'))

// Local prototypes.
static void commPutStr(const char * str, int fd);
static void randomPrintableString(char * str, int bufSize);
static void randomAsciiString(char * str, int bufSize);

int main(int argc, char * argv[])
{
    char str[kMaxString+1];
    int i;
    int j;
    int len;

    // for (i=0;i<32;i++)
    // {
    //     randomPrintableString(cmd, sizeof(cmd));
    //     printf("\"%s\"\n", cmd);
    // }
    // return 0;

    // for (i=0;i<100;i++)
    // {
    //     char rp = RANDOM_NONSPACE_PRINTABLE;
    //     char rc = RANDOM_NONZERO_ASCII;
    //     char rn = RANDOM_NUMER_CHAR;
    //     printf("'%c'  '%02X'  '%c'\n", rp, rc, rn);
    // }
    // return 0;

    // for(i=0;i<100;i++)
    // {
    //     randomPrintableString(cmd, sizeof(cmd));
    //     printf("\"%s\"\n", cmd);
    // }
    // return 0;

    if (argc < 2)
    {
        puts("Syntax: fuzzer <seed #>");
        exit(-2);
    }

    // Just get a seed value from the command-line parameter, assumed to be a number.
    unsigned int seed = atoi(argv[1]);
    srand(seed);

    int fd = comms_open(devstr);
    if (fd < 0)
    {
        printf("Failed to open \"%s\"\n", devstr);
        return -1;
    }

    printf("Using serial I/O through \"%s\"\n", devstr);

    /***** Fuzz the input stream *****/

    // Test random commands, random lengths, and random number of string parameters, random length.
    for (i=0;i<kMaxCommands;i++)
    {
        randomPrintableString(str, sizeof(str));
        printf("\t===== String of length %d ======\n", strlen(str));
        commPutStr(str, fd);

        int pidx;
        for (pidx=0;pidx<kMaxParams;pidx++)
        {
            commPutStr(" ", fd);
            randomPrintableString(str, sizeof(str));
            commPutStr(str, fd);
        }
        commPutStr("\r\n", fd);

        sleep(1);
    }


    /***** Fuzz the handler registration *****/

    // Test NULL command pointer, NULL handler pointer, NULL parameter pointer, zero parameters.

    // Test num parameters greater than parameters pointed to.

    // Test negative number of parameters given.


    /***** Other *****/

    // Test too many registered handlers.

    // Test differen line-ends.

    comms_close(fd);

    return 0;
}

/**
 * Send string out the comm port.
*/
static void commPutStr(const char * str, int fd)
{
    int len = strlen(str);
    int i;
    for (i=0;i<len;i++)
        comms_put(str[i], fd);
        // printf("%c", str[i]);
}

/**
 * Create a random string, consisting of printable characters (no spaces), of random length.
 * String will always have at least one character, guaranteed to fit in buffer, with room for null terminator.
*/
static void randomPrintableString(char * str, int bufSize)
{
    memset(str, 0, bufSize);
    int len = rand() * (bufSize-2) / RAND_MAX + 1;
    int i;
    for (i=0;i<len;i++)
        str[i] = RANDOM_NONSPACE_PRINTABLE;
}

/**
 * Create a random string, consisting of 7-bit (non-zero) ASCII values, of random length.
 * String will always have at least one character, guaranteed to fit in buffer, with room for null terminator.
*/
static void randomAsciiString(char * str, int bufSize)
{
    memset(str, 0, bufSize);
    int len = rand() * (bufSize-2) / RAND_MAX + 1;
    int i;
    for (i=0;i<len;i++)
        str[i] = RANDOM_NONZERO_ASCII;
}