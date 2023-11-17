/**
 * @file loopback.c
 * @author tom@gordoninnovations.com
 * @brief Posix-compliant routines for opening, closing and character I/O for a loop-back device.
 * @version 0.1
 * @date 2023-10-20
 * 
 * @copyright Copyright (c) 2023
 * 
 * Testing using a PC:
 * For Linux, we can't just capture keystrokes from the terminal, since the terminal itself buffers input
 * until a carriage-return (enter key), and so this won't let us receive characters one by one.
 * So, for Linux, we instead use the "socat" command (may require installing) to provide a software
 * serial loop-back, then use a terminal monitor ("screen" works for this, but may require installing)
 * to input characters and observe the reponses. The "loopback.sh" and "unlink_loopback.sh" scripts
 * are provided to help set this up and tear it down.
 * 
 */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "loopback.h"

// Local prototypes.
// static void reset(int fd);

/**
 * @brief Open the serial port indicated by the given string.
 * If devStr given as NULL, assumes re-opened after a previous call.
 * This is to allow resetting the port by simply closing, then calling this with NULL.
 * 
 * @param devStr serial device to open, as "/dev/pts/1"
 * @return integer file descriptor for stream opened, or -1 on fail
 */
int loopback_open(const char * devStr)
{
  static char _devStr[1024];
  // Open stream for read/write, so we can use it both as 
  // stdin and stdout.
  if (devStr != NULL)
    strcpy(_devStr, devStr);  // if non-null string passed, save it in case of re-open later

  int fd = open(_devStr, O_RDWR | O_NOCTTY);

  // Open open or reopen device string given or saved from last time.
  return fd;
}

/**
 * @brief Close the given serial stream. Ignores if NULL.
 * 
 * @param fd file descriptor for file stream to close
 */
void loopback_close(int fd)
{
  // Close FILE pointer if not invalid.
  if (fd >= 0)
    close(fd);
}

/**
 * @brief Wait for and get the next character from the serial stream. 
 * 
 * @param fd file descriptor for file stream to close
 * @return character read
 */
char loopback_get(int fd)
{
  if (fd >= 0)
  {
    char c;
    read(fd, &c, 1);
    return c;
  }
  return '\0';
}

/**
 * @brief write the given character to a serial stream.
 * Flushes character immediately to the stream.
 * 
 * @param c character to write
 * @param fd file descriptor for file stream to close
 */
void loopback_put(char c, int fd)
{
  // Poop it out and flush.
  if (fd >= 0)
  {
    write(fd, &c, 1);
  }
}

/**
 * @brief Reset the I/O stream.
 * This is a bit sketchy, as it assumes that the FILE pointer when reopened
 * does not change, but it seems to "unlatch" the port when it starts returning FFFFFFFF
 * continuously for some reason. clearerr() does not seem to fix.
 * 
 * @param fd file descriptor for file stream to close
 */
// static void reset(FILE * fp)
// {
//   // loopback_close(fp);
//   // loopback_open(NULL);
//   if (ferror(fp))
//     clearerr(fp);
// }
