/**
 * @file loopback.c
 * @author tom@gordoninnovations.com
 * @brief Posix-compliant routines for opening, closing and character I/O for a loop-back device.
 * @version 0.1
 * @date 2023-10-20
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include "loopback.h"

// Local prototypes.
static void reset(FILE * fp);

/**
 * @brief Open the serial port indicated by the given string.
 * If devStr given as NULL, assumes re-opened after a previous call.
 * This is to allow resetting the port by simply closing, then calling this with NULL.
 * 
 * @param devStr serial device to open, as "/dev/pts/1"
 * @return FILE* pointer to file stream opened, or NULL on fail
 */
FILE * loopback_open(const char * devStr)
{
  static char _devStr[1024];
  // Open stream for read/write, so we can use it both as 
  // stdin and stdout.
  if (devStr != NULL)
    strcpy(_devStr, devStr);  // if non-null string passed, save it in case of re-open later

  // Open open or reopen device string given or saved from last time.
  return fopen(_devStr, "r+");
}

/**
 * @brief Close the given serial stream. Ignores if NULL.
 * 
 * @param fp pointer to file stream to close
 */
void loopback_close(FILE * fp)
{
  // Close FILE pointer if not NULL.
  if (fp != NULL)
    fclose(fp);
}

/**
 * @brief Wait for and get the next character from the serial stream. 
 * 
 * @param fp pointer to file stream
 * @return character read
 */
char loopback_getc(FILE * fp)
{
  if (fp != NULL)
  {
    char c = -1;
    while (c == -1)
    {
      c = fgetc(fp);
      if (c < 0)
      {
        // if (feof(fp))
        //   putchar('o'); // EOF
        // else if (ferror(fp))
        //   putchar('x'); // error
        // else
        //   putchar('?'); // unknown fail
        reset(fp);
        continue;
      }
    }
    return c;
  }
  return '\0';
}

/**
 * @brief write the given character to a serial stream.
 * Flushes character immediately to the stream.
 * 
 * @param c character to write
 * @param fp pointer to a file stream
 */
void loopback_putc(char c, FILE * fp)
{
  // Poop it out and flush.
  if (fp != NULL)
  {
    fputc(c, fp);
    fflush(fp);
  }
}

/**
 * @brief Reset the I/O stream.
 * This is a bit sketchy, as it assumes that the FILE pointer when reopened
 * does not change, but it seems to "unlatch" the port when it starts returning FFFFFFFF
 * continuously for some reason. clearerr() does not seem to fix.
 * 
 * @param fp pointer to file stream to reset
 */
static void reset(FILE * fp)
{
  loopback_close(fp);
  loopback_open(NULL);
}



#ifdef STANDALONE
int main(int argc, char * argv[])
{
  char const * const devstr = "/dev/pts/1";
  FILE * fp = loopback_open(devstr);
  if (fp == NULL)
  {
    printf("Failed to open \"%s\"\n", devstr);
    return -1;
  }

  char c = ' ';
  while (c != 0x1B)
  {
    c = loopback_getc(fp);
    loopback_putc(c, fp);
    printf("%02X ", (uint16_t)c);
    // putchar(c);
    // if (c == '\r')
    //   putchar('\n');    // auto line-feed w/carriage return
  }

  loopback_close(fp);

  return 1;
}
#endif // STANDALONE
