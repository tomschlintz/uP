/**
 * @file comms.c
 * @author tom@gordoninnovations.com
 * @brief Posix-compliant routines for opening, closing and character I/O for a loop-back device.
 * @version 0.1
 * @date 2023-10-20
 * 
 * @copyright Copyright (c) 2023, Gordon Innovations
 * 
 * Testing using a PC:
 * For Linux, we can't just capture keystrokes from the terminal, since the terminal itself buffers input
 * until a carriage-return (enter key), and so this won't let us receive characters one by one.
 * So, for Linux, we instead use the "socat" command (may require installing) to provide a software
 * serial loop-back, then use a terminal monitor ("screen" works for this, but may require installing)
 * to input characters and observe the reponses. The "loopback.sh" and "unlink_loopback.sh" scripts
 * are provided to help set this up and tear it down.
 * 
 * There are some subtle differences between Linux and Windows implementations, which are conditionally
 * compiled based on the definition of __linux__ at compile-time. Otherwise we strive to use the least
 * common denominator between the two OSes, and Posix-compliance where possible.
 * 
 * For testing in Linux, recommend calling loopback.sh for using socat to set up a software serial loopback,
 * then calling screen /dev/pts/<#> for a simple terminal emulation.
 * 
 * For testing in Windows, recommend using Putty or other terminal program, and com0com or other software
 * serial loop-back utility.
 * 
 * Alternatively, for either OS, a dedicated terminal application could be created using this file, along
 * with a HW or SW loop-back solution, to test.
 */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include "comms.h"

#ifdef __linux__
#else
#define O_NOCTTY _O_BINARY
#endif // __linux__

/**
 * @brief Open the serial port indicated by the given string.
 * If devStr given as NULL, assumes re-opened after a previous call.
 * This is to allow resetting the port by simply closing, then calling this with NULL.
 * TODO: no longer need this option.
 * 
 * @param devStr serial device to open, as "/dev/pts/1"
 * @return integer file descriptor for stream opened, or -1 on fail
 */
int comms_open(const char * devStr)
{
  static char _devStr[1024];
  // Open stream for read/write, so we can use it both as 
  // stdin and stdout.
  if (devStr != NULL)
    strcpy(_devStr, devStr);  // if non-null string passed, save it in case of re-open later

  int fd = open(_devStr, O_RDWR | O_NOCTTY);

  // Open or reopen device string given or saved from last time.
  return fd;
}

/**
 * @brief Close the given serial stream. Ignores if NULL.
 * 
 * @param fd file descriptor for file stream to close
 */
void comms_close(int fd)
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
char comms_get(int fd)
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
void comms_put(char c, int fd)
{
  // Poop it out and flush.
  if (fd >= 0)
  {
    write(fd, &c, 1);
  }
}
