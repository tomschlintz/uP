/**
 * @file chell.c
 * @author Tom Gordon
 * @brief Contains processChar(), which accepts a character from stdin and returns a character for stdout (or uses a call-back).
 * Provides a full-featured, Linux-style shell interface, including handling of backspace, function and arrow keys for line navigation and line recall.
 * Allows registering of shell commands, including progressive help and function handling for each. Each handler will receive the command string,
 * Number of parameters, and parameter list. Automatically provide a "help" shell command, based on the help provided for each command registered.
 * 
 * FUTURE: also provide a call-back to provide parameter hints.
 * 
 * Written in standard C and uses only standard library headers, minimum RAM, to allow integration into even the smallest projects.
 *
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include "chell.h"

#define STAND_ALONE   // define to include a main() function, for stand-alone testing

typedef struct
{
    const char * cmd;
    void (*handler)(char const * const cmd, char const * const * param, int numParams);
    char const * help;
    // future: list of string pointers, where strings are either space-separated parameter hints (e.g. "left middle right") or ghost hints
    // as to the variable type expected (e.g. "<x coord>")
    char const * const * hints;
} Cmd_struct;

#ifndef len
    #define len(array) (sizeof(array)/sizeof(array[0]))
#endif

// Local prototypes.
static bool processLine(char * line);
static void outChar(const char c);
static bool isEmptyLine(const char * line);
static void handle_unhandled(char const * const cmd, char const * const * param, int numParams);
static void handle_help(char const * const cmd, char const * const * param, int numParams);
static void chell_printf(char * fmt, ...);

// File globals.
static char g_outLineEnd[3] = {0};              // preferred line-end character(s) to output
static char g_prompt[MAX_SHELL_PROMPT+1] = {0}; // prompt to return to outgoing stream, or empty string if none
static bool g_lineEndSet = false;               // set true only after line-end initialized
static bool g_helpInitialized = false;          // help command list has been initialized, to include help handler
static Cmd_struct g_cmd[MAX_COMMANDS] = { 0 };  // list of commands, as registered
static int g_numRegCmds = 0;                    // the number of commands registered, including the standard help
static void (*g_cb_out)(const char c) = NULL;   // if used, allows feeding characters to output through a call-back function - set to NULL if not used
static char g_outCharsBuf[MAX_STR+1] = { 0 };   // buffer to hold stdout characters until return
static int g_outCharIdx = 0;                    // next index into outCharBuf[] - empty if zero

bool chell_RegisterHandler(const char * cmd, void (*handler)(char const * const cmd, char const * const * param, int numParams), const char * help, char const * const * hints)
{
  //  a) One full help string, which gets displayed when "help" shows all the commands.
  //  b) A list of pointers to help strings (null-termminated) that allows guessing the next parameter from what the user starts typing, with tab.
  //     These strings could be space-separated words that are valid for that parameter. For example, a "move" command might take as a 1st parameter
  //     either "waist", "shoulder" or "arm", so one string in the list might be provided as "waist shoulder arm".
  //  c) If a parameter is an integer or floating-point variable, help should hint at what type of value is to be entered. For example, a "move" command
  //     might take <x> <y> <z> coordinates, so  entering "move", then tab, should display something like " <x coord>", but leave the cursor on the "<",
  //     and remove this as soon as more characters are entered. Distinction could be that start/end bracketing characters are given ("<>", "[]", "{}"),
  //     then assume this is a "ghost hint" rather than acceptable string choices. Null pointer provides no hints for that command.
  //  B) and C) may be added later.
  // If not called at all, default will be to provide just the help handler.

  // If not used, both "help" and "hints" may be passed as null. Define pseudonyms for these in header.

  // Note that handler routines, help string and hint strings are all just pointers to what's declared by the caller.

  // Could allow space-separated string for cmd parameter, to register multiple command for one handler. For example, the caller may elect to use
  // one handler routine for multiple commands, handling differently based on the "cmd" string passed, as "move skip jump" may all point to the same handler.

  // We must provide helper functions, and template examples, so that caller has the flexibility to handle commands and parameters as they see fit,
  // but does not over-burden caller with having to deal with too much complexity in each of their hander functions. For example, provide calls for:
  //  a) Verifying the number and type of parameters, according to how the command was registered.
  //  b) Probably just need to show template examples of how to handle strings vs int vs float parameters, along with range checking, and how to 
  //     use the standard handler function parameters provided.

  // Fail-safe: check for max. Ignore and retun failure if full.
  if (g_numRegCmds >= MAX_COMMANDS)
    return false;

  // Fail-safe: ignore and return failure if handler pointer is null.
  if (handler == NULL)
    return false;

  // If first time called, then initialize list to include a "help" handler.
  if (!g_helpInitialized)
  {
    // Recursively call this to register the standard help handler. Flag initialized _first_ to avoid infinite recursion!
    g_helpInitialized = true;
    chell_RegisterHandler("help", handle_help, "this help message", NULL);
  }

  g_cmd[g_numRegCmds].cmd = cmd;
  g_cmd[g_numRegCmds].handler = handler;
  g_cmd[g_numRegCmds].help = help;
  g_cmd[g_numRegCmds].hints = hints;

  // Count commands, and return success.
  g_numRegCmds++;
  return true;
}

/**
 * @brief Call with each character received from an input stream, one character per call.
 * Recognizes and processes commands with parameters (based on handlers registered), with each line ending with one or more line-end characters.
 * Recognizes and handles backspace characters and escape sequences.
 * Uses cb_out call-back to echo received, and process backspace and escape sequences.
 * 
 * @param c ASCIIZ character that is next in stdin stream of characters to process
 * @param cb_out call-back to stdout stream - putchar works nicely, if available
 * @return char* ASCIIZ string received, or NULL if complete string and line-end not received yet
 */
char * chell_ProcessChar(const char c, int (*cb_out)(int c))
{
  const char kUpArrowEscape[] = "\x1B\x5b\x41";
  const char kDownArrowEscape[] = "\x1B\x5b\x42";
  const char kRightArrowEscape[] = "\x1B\x5b\x43";
  const char kLeftArrowEscape[] = "\x1B\x5b\x44";
  const char kF3Escape[] = "\x1B\x4F\x52";
  static char buf[MAX_TOTAL_COMMAND_CHARS+1] = { 0 };
  static char history[MAX_HISTORY][MAX_TOTAL_COMMAND_CHARS+1] = { 0 };
  static int hidx = 0;
  static int idx = 0;
  static char escapeChars[3] = { 0 };
  static bool lineEnding = false;   // set true if currently "line ending", to ignore further line-end characters

  // Copy pointer to character output call-back as file global, for use by other functions herein.
  g_cb_out = (void(*)(char))cb_out;

  // If not otherwise called, default our preferred line-end we write out to CRLF.
  if (!g_lineEndSet)
  {
    g_lineEndSet = true;
    strcpy(g_outLineEnd, "\r\n");
  }

  // If chell_RegisterHandler() was never called to register any user commands, then initialize it now so that at least "help" is handled.
  // Maybe make it a special help command, to provide help on how to register commands ?
  if (!g_helpInitialized)
  {
    // Register the built-in help handler. Must flag as initialized first, to avoid re-adding in register call.
    g_helpInitialized = true;
    chell_RegisterHandler("help", handle_help, "this help message", NULL);
  }

  // Ignore subsequent line-end characters if one already processed.
  // Note we would have already returned the string processed with our first line-end character processed.
  // TODO: we may need to accommodate multiple, single returns hit (blank lines).
  if (lineEnding && ((c == 0x0D) || (c == 0x0A)))
    return NULL;

  // If here, then not a line-end character, or not already "line ending", so reset that flag.
  lineEnding = false;

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
      chell_printf("\x08 \x08");  // backspace, clear, backspace again
      idx--;
    }
  } else if ((c == 0x0D) || (c == 0x0A))
  {
    /** Line-end received, command has been entered. **/

    // Terminate buffer, and echo line-end.
    buf[idx] = '\0';  // make sure we're terminated
    idx = 0;          // reset for next command

    if (!isEmptyLine(buf))
    {
      // Put a line between what was just entered and whatever output the response will be, unless CR only entered.
      chell_printf(g_outLineEnd);

      // Parse and process string received.
      if (processLine(buf))
      {
        // Track successful command history.
        memcpy(history[hidx], buf, sizeof(history[0]));
        hidx = (hidx + 1) % MAX_HISTORY;
      }
    }

    // Prompt
    chell_printf("%s%s", g_outLineEnd, g_prompt);
  } else if ((c >= ' ') && (c <= '~'))
  {
    // Otherwise buffer and echo any printable character. Don't allow index beyond buffer bytes - 1, to allow for NULL-terminator.
    if (idx < (MAX_TOTAL_COMMAND_CHARS))
    {
      buf[idx] = (char)c;
      cb_out(c);
      idx++;
    }
  }

  // Return any pending output characters, otherwise return an empty string.
  if (g_outCharIdx > 0)
  {
    g_outCharsBuf[g_outCharIdx] = '\0';   // make sure string is null-terminated
    g_outCharIdx = 0;                     // reset buffer index
    return g_outCharsBuf;
  } else
  {
    return "";
  }
}

/**
 * @brief Set the preferred line-end characters for stdout. Default is a single line-feed.
 * May be "\r", "\n" or "\r\n", or any string of up to two characters.
 * 
 * @param c preferred line-end characters, max two characters
 */
void chell_setOutLineEnd(const char * str)
{
  memset(g_outLineEnd, 0, sizeof(g_outLineEnd));      // pre-clear string
  strncpy(g_outLineEnd, str, sizeof(g_outLineEnd)-1); // copy up to two characters for line-end
}

/**
 * @brief Set a prompt string to feed back to the outgoing stream.
 * 
 * @param str prompt string, as "> "
 */
void chell_setPrompt(const char * str)
{
  memset(g_prompt, 0, sizeof(g_prompt));
  strncpy(g_prompt, str, MAX_SHELL_PROMPT);
}

/**
 * @brief Check number of parameters, based on given criteria, show error and return false if bad.
 * 
 * @param numGivenParams the number of parameters given
 * @param numExpectedParams the number of parameters expected
 * @return true if parameters are good, false if bad
 */
bool chell_confirmParameters(int numGivenParams, int numExpectedParams)
{
  if (numGivenParams < numExpectedParams)
  {
    chell_printf("*** You only gave me %d parameters, I need at least %d ***\n", numGivenParams, numExpectedParams);
    return false;
  }

  return true;
}

/**
 * @brief Parses complete string received, then identifies and processes command, with parameters.
 * This will change the line buffer passed, as it is parsed. In fact, the pointers to the command and each
 * parameter string are actually just pointers into the line string. This works for a single-threaded system,
 * as long as the command and parameter strings are used by the handler before any more characters are processed,
 * since this begins to overwrite that line again.
 * 
 * @param line string received - modified when parsed by strtok()
 * @return true 
 * @return false 
 */
static bool processLine(char * line)
{
    char * p;
    char * cmd;                             // command string (first parsed string)
    char const * param[MAX_PARAMETERS];     // list of parameters parsed for command line
    int numParams = 0;

    // Nothing to parse if empty string, or contains only line-end characters.
    if ((line == NULL) || (line[0] == '\0'))
        return false;

    // Split the copied string into command and parameters by inserting
    // a NULL for each token, and assigning pointer to each. Note this does not
    // add to the line passed, only replaces the token characters with NULL.
    p = strtok(line, " ,");
    cmd = p;
    int i;
    for (i=0;i<len(param) && p!=NULL;i++)
    {
        p = strtok(NULL, " ,");
        if (p != NULL)
        {
            param[i] = p;
            numParams++;
        }
    }

    // Loook for match in g_cmd table, and call handler if found.
    for (i=0;i<len(g_cmd);i++)
    {
        if ((g_cmd[i].cmd != NULL) && (strcmp(cmd, g_cmd[i].cmd) == 0))
        {
            g_cmd[i].handler(cmd, param, numParams);
            break;
        }
    }

    if (i >= len(g_cmd))
      handle_unhandled(cmd, param, numParams);
}

/**
 * @brief Look for escape sequences: known strings starting with an escape character (0x1B).
 * These include up, down, left and right arrow keys, and various function keys.
 * The return value indicates the state of gathering an escape sequence:
 *   -1 : processing a sequence, not complete : caller should discard the incoming character
 *    0 : no escape seuence is being gathered : caller should process the character as normal
 *  1-n : an escape sequence just recognized : caller should take action based on the return value, ignoring incoming character
 * 
 * @param c next character to process
 * @return int 0 if not collecting escape characters, -1 if in process of collecting an escape sequence, or 1, 2, 3, 4 for up/down/left/right escape detected
 */
int processEscapes(char c)
{
  char const * const kEscapes[] =
  {
    "\x1B\x5b\x41",     // up arrow
    "\x1B\x5b\x42",     // down arrow
    "\x1B\x5b\x43",     // right arrow
    "\x1B\x5b\x44",     // left arrow
    "\x1B\x4F\x52",     // F3
  };
  static char escapeChars[3] = { 0 };
  int numEscapes = sizeof(kEscapes) / sizeof(kEscapes[0])

  if (c == 0x1B)
  {
    bool doubleEscape = escapeChars[0] == 0x1B;
    memset(escapeChars, 0, sizeof(escapeChars));

    if (doubleEscape)
    {
      // If two escapes in a row (one already buffered), tell caller to process eescape as a regular character,
      // and ignore it here.
      return 0;
    } else
    {
      // Otherwise start buffer with it here, and tell caller that we're currently gathering a possible escape sequence.
      escapeChars[0] = c;
      return -1;
    }
  } else
  {
    if (escapeChars[1] == 0)
    {
      // If no  match for 2nd character in our escape table, then reset and tell call to treat as a regular character.
      int i;
      for (i=0;i<numEscapes;i++)
        if (kEscapes[i][1] == c)
          break;
      if (i >= numEscapes)
      {
        memset(escapeChars, 0, sizeof(escapeChars));
        return 0;
      }

      // Otherwise buffer and tell caller we're working on an escape sequence.
      escapeChars[1] = c;   // 2nd character
      return -1;
    } else
    {
      // If match to known sequences, reset and return 1-based index for matched sequence.
      int i;
      for (i=0;i<numEscapes;i++)
      {
        if (memcmp(escapeChars, kEscapes, 3) == 0)
        {
          memset(escapeChars, 0, sizeof(escapeChars));
          return i + 1;
        }
      }

      // If no match to known sequences, reset, and tell caller to ignore the character (treat as valid escape, just now handled here).
      memset(escapeChars, 0, sizeof(escapeChars));
      return 0;
    }
  }
}

/**
 * @brief Uses the given call-back to feed characters back to caller's stdout, either by calling
 * given call-back function, or buffering until return (if cb_out is NULL).
 * 
 * @param c character to try to write to caller's stdout
 */
static void outChar(const char c)
{
  // If given, use call-back to send character to stdout
  if (g_cb_out)
  {
    g_cb_out(c);
  } else
  {
    // Otherwise, buffer and return as a string of one or more characters.
    if (g_outCharIdx < MAX_STR)
      g_outCharsBuf[g_outCharIdx++] = c;
  }
}

/**
 * @brief Reveals if given line is "empty".
 * Line is considered empty if it has no string to try to process.
 * 
 * @param line line string to check
 * @return true if line has no command to process, i.e., is "empty"
 */
static bool isEmptyLine(const char * line)
{
  int len = strlen(line);
  int i;
  for (i=0;i<len;i++)
    if ((line[i] != '\r') && (line[i] != '\n') && (line[i] != ' '))
      return false;
  return true;
}

/**
 * @brief Build-in default handler when no other handler found in command table.
 * 
 * @param cmd command string
 * @param param list of pointers to parameter strings - use as param[0], param[1] ...
 * @param numParams number of parameters parsed for this command
 */
static void handle_unhandled(char const * const cmd, char const * const * param, int numParams)
{
  (void)cmd;
  (void)param;
  (void)numParams;
  chell_printf("*** Huh? ***%s", g_outLineEnd);
}

/**
 * @brief Built-in handler to display help on all register commands.
 * 
 * @param cmd command string
 * @param param list of pointers to parameter strings - use as param[0], param[1] ...
 * @param numParams number of parameters parsed for this command
 */
static void handle_help(char const * const cmd, char const * const * param, int numParams)
{
  (void)cmd;
  (void)param;
  (void)numParams;

  chell_printf("%s===== Commands =====%s\n", g_outLineEnd, g_outLineEnd);
  int i;
  for (i=0;i<g_numRegCmds; i++)
  {
    char const * helpText = "";
    if (g_cmd[i].help != NULL)
      helpText = g_cmd[i].help;
    chell_printf("  \"%s\" - %s%s", g_cmd[i].cmd, helpText, g_outLineEnd);
  }
}

void handle_example(char const * const cmd, char const * const * param, int numParams)
{
  // Verify the correct number of parameters.
  if (!chell_confirmParameters(numParams, 2))
    return;

  int val1 = atoi(param[0]);
  int val2 = atoi(param[1]);
  chell_printf("The sum of %d + %d = %d\r\n", val1, val2, val1 + val2);
}

/**
 * @brief Format and output to stream, using call-back given.
 * 
 * @param fmt 
 * @param ... 
 */
void chell_printf(char * fmt, ...)
{
  va_list args;
  char str[MAX_TOTAL_COMMAND_CHARS+1];

  va_start(args, fmt);
  vsprintf(str, fmt, args);
  va_end(args);

  int len = strlen(str);
  int i;
  for (i=0;i<len;i++)
    outChar(str[i]);
}


#ifdef STAND_ALONE
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "loopback.h" // for testing with a loop-back connection

// Loop-back serial device strings. This is for stand-alone testing. The caller
// into this library is expected to both provide characters from an incoming stream
// and provide a call-back to allow returning characters to an outgoing stream. This library
// does not do streams, it just processes incoming characters and parses and processes commands.
#ifdef __linux__
char const * const devstr = "/dev/pts/1";
#else
char const * const devstr = "COM1";
#endif // __linux__

FILE * fp = NULL;

// Use loopback out as our output call-back.
int cb(int c) { loopback_putc(c, fp); }

/**
 * @brief Main entry function, for testing via command-line.
 */
int main(int argc, char * argv[])
{
  fp = loopback_open(devstr);
  if (fp == NULL)
  {
    printf("Failed to open \"%s\"\n", devstr);
    return -1;
  }

  printf("Using serial I/O through \"%s\"\n", devstr);

  chell_RegisterHandler("add", handle_example, "add two numbers", NULL);
  chell_setPrompt("> ");

  char c = ' ';
  while (c != 0x1B)
  {
    // Get the next character in.
    c = loopback_getc(fp);

    // Process.
    chell_ProcessChar(c, cb);
  }

  loopback_close(fp);

  chell_printf(g_outLineEnd);
  return 0;
}
#endif  // STAND_ALONE