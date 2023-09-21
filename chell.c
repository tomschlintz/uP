/**
 * @file meshell.c
 * @author Tom Gordon
 * @brief Contains processStream(), which accepts a character from stdin and returns a character for stdout (or uses a call-back).
 * Provides a full-featured shell interface, including handling of backspace, function and arrow keys for line navigation and line recall.
 * Allows registering of shell commands, including progressive help and function handling for each. Each handler will receive the command string,
 * Number of parameters, and parameter list. Automatically provide a "help" shell command, based on the help provided for each command registered.
 * 
 * Written in standard C and uses only standard library headers, minimum RAM, to allow integration into even the smallest projects.
 *
 * @version 0.1
 * @date 2023-09-18
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

const int MAX_CMD = 64;

typedef struct
{
    const char * cmd;
    void (*handler)(char const * const cmd, char const * const * param, int numParams);
    const char * help;
} Cmd_struct;

bool registerHandler(void (*handler)(char const * const cmd, char const * const * param, int numParams), const char * strCmd, const char * strSyntax)
{
  //TODO - need a way to specify not only the command string, but a help string as well, which can be parsed to be able to
  // show progressive help as user enters only some of the required parameters.
}

/**
 * @brief Call from main loop as often as possible, looks for incoming serial characters, echos, 
 * and allows for backspace to retype and escape to start over. Returns NULL until c/r received.
 * 0x1B 0x5B 0x41 escape sequence = up arrow
 * 0x1B 0x4F 0x52 escape sequence = F3
 * 
 * @param c ASCIIZ character that is next in stream of characters to process
 * @return char* ASCIIZ string received, or NULL if complete string and line-end not received yet
 */
char * processStream(const char c)
{
  const char * kLineEnd = "\r\n";
  const char kUpArrowEscape[] = "\x1B\x5b\x41";
  const char kDownArrowEscape[] = "\x1B\x5b\x42";
  const char kRightArrowEscape[] = "\x1B\x5b\x43";
  const char kLeftArrowEscape[] = "\x1B\x5b\x44";
  const char kF3Escape[] = "\x1B\x4F\x52";
  static char buf[MAX_CMD] = { 0 };
  const int MAX_HISTORY = 4;
  static char history[MAX_HISTORY][MAX_CMD] = { 0 };
  static int hidx = 0;
  static int idx = 0;
  static char escapeChars[3] = { 0 };

  // Capture remainder of 3-character escape sequences, if escape (0x1B) has already been captured.
  if (escapeChars[0] == 0x1B)
  {
    if (escapeChars[1] == 0)
    {
      escapeChars[1] = c;   // 2nd character
    } else
    {
      escapeChars[2] = c;   // 3rd and final character
      if ( (memcmp(escapeChars, kUpArrowEscape, sizeof(escapeChars)) == 0) || (memcmp(escapeChars, kF3Escape, sizeof(escapeChars)) == 0) )
      {
        // For up arrow and F3, replace buffer with previous in history, and replace string at prompt.
        if (--hidx < 0)
          hidx = MAX_HISTORY - 1;
        memcpy(buf, history[hidx], sizeof(buf));
        buf[MAX_CMD-1] = '\0';
        idx = strlen(buf);
        int i;
// TODO: need some mechanism for echoing characters to system-defined outgoing stream.
        for (i=0;i<MAX_CMD;i++)
          Serial.write(0x08);
        for (i=0;i<MAX_CMD;i++)
          Serial.write(' ');
        for (i=0;i<MAX_CMD;i++)
          Serial.write(0x08);
        for (i=0;i<idx;i++)
          Serial.write(buf[i]);
      }

      if ((memcmp(escapeChars, kLeftArrowEscape, sizeof(escapeChars)) == 0) && (idx > 0))
      {
        // For left arrow, move cursor and index left, to start of string.
        Serial.write(0x08);
        idx--;
      }

      if ((memcmp(escapeChars, kRightArrowEscape, sizeof(escapeChars)) == 0) && (idx < (MAX_CMD-2)) && (buf[idx] != '\0'))
      {
        // For right arrow, move cursor and index right, to just past end of string currently buffered.
        Serial.write(0x08);
        idx--;
      }

      memset(escapeChars, 0, sizeof(escapeChars));
    }
    return NULL;  // no more to do until next character or c/r entered
  }

  if (c == 0x1B)
  {
    // With escape character, clear escape buffer, then capture escape (0x1B) as the first in the sequence.
    memset(escapeChars, 0, sizeof(escapeChars));
    escapeChars[0] = c;
    return NULL;  // no more to do until next character or c/r entered
  } else if ((c == 0x7F) || (c == 0x08))
  {
    // Handle backspace (0x08) or del (0x7F)
    if (idx > 0)
    {
      Serial.write(0x08);
      Serial.write(' ');
      Serial.write(0x08);
      idx--;
    }
  } else if ((c == 0x0D) || (c == 0x0A))
  {
    // Flush receive buffer to clear out any remaining line-end characters.
    while (Serial.available() > 0)
      Serial.read();

    // Reset and return buffer on c/r or l/f.
    buf[idx] = '\0';  // make sure we're terminated
    idx = 0;          // reset for next command
    Serial.write(kLineEnd); // echo the line-end
    memcpy(history[hidx], buf, sizeof(history[0]));
    hidx = (hidx + 1) % MAX_HISTORY;  //TODO: always place latest line at end of history
    return buf;
  } else if ((c >= ' ') && (c <= '~'))
  {
    // Otherwise buffer and echo any printable character. Don't allow index beyond buffer bytes - 1, to allow for NULL-terminator.
    if (idx < (MAX_CMD-1))
    {
      buf[idx] = (char)c;
      Serial.write(c);
      idx++;
    }
  }

  // No c/r yet.
  return NULL;
}

/**
 * @brief Main entry function, for testing via command-line.
 */
int main(int argc, char * argv[])
{
  // Register set of test commands.
  // Accept and process input, echo output, until escape key hit.
}
