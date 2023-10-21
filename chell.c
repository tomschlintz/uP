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

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include "chell.h"

#define STAND_ALONE   // define to include a main() function, for stand-alone testing

#define MAX_STR 64      // maximum command or parameter string expected, for sizing arrays.
#define MAX_PARAMETERS 16   // maximum parameter strings - note this will multiply by MAX_STR when allocating string storage!
#define MAX_TOTAL_COMMAND_CHARS ((MAX_PARAMETERS+1) * MAX_STR + MAX_PARAMETERS)   // string length enough for command and parameters, including a space between each
#define MAX_HISTORY 16  // depth of recall history
#define MAX_COMMANDS 64 // maximum number of commands that may be registered

typedef struct
{
    const char * cmd;
    void (*handler)(char const * const cmd, char const * const * param, int numParams);
    char const * help;
    // future: list of string pointers, where strings are either space-separated parameter hints (e.g. "left middle right") or ghost hints
    // as to the variable type expected (e.g. "<x coord>")
    char const * const * hints;
} Cmd_struct;

// Local prototypes.
static bool processLine(char const * const line);
static void outChar(const char c);
static void handle_help(char const * const cmd, char const * const * param, int numParams);
static void cbPrintf(char * fmt, ...);

// File globals.
static char g_outLineEnd[3] = {0};             // preferred line-end character(s) to output
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
    chell_RegisterHandler("help", handle_help, NULL, NULL);
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
    chell_RegisterHandler("help", handle_help, NULL, NULL);
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
      cbPrintf("\x08 \x08");  // backspace, clear, backspace again
      idx--;
    }
  } else if ((c == 0x0D) || (c == 0x0A))
  {
    /** Line-end received, command has been entered. **/

    // Terminate buffer, and echo line-end.
    buf[idx] = '\0';  // make sure we're terminated
    idx = 0;          // reset for next command

    // Parse and process string received.
    if (processLine(buf))
    {
      // Track successful command history.
      memcpy(history[hidx], buf, sizeof(history[0]));
      hidx = (hidx + 1) % MAX_HISTORY;
    }

    // Prompt
    cbPrintf(g_outLineEnd);
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
 * @brief Parses complete string received, then identifies and processes command, with parameters.
 * 
 * @param line string received
 * @return true 
 * @return false 
 */
static bool processLine(char const * const line)
{
  printf("Processing line \"%s\"\n", line);



  // command not valid: return failure.
  return false;
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
 * @brief Built-in handler to display help on all register commands.
 * 
 * @param cmd command string, e.g. "move" - passed to all command handlers
 * @param param pointer to list of parameter strings - passed to all command handlers
 * @param numParams the number of parameter strings - passed to all command handlers
 */
static void handle_help(char const * const cmd, char const * const * param, int numParams)
{
  (void)cmd;
  (void)param;
  (void)numParams;

  cbPrintf("Built %s %s. Commands:%s", __DATE__, __TIME__, g_outLineEnd);
  int i;
  for (i=0;i<g_numRegCmds; i++)
      cbPrintf("\t%s %s%s", g_cmd[i].cmd, g_cmd[i].help, g_outLineEnd);
}

/**
 * @brief Format and use call-back to output to stream.
 * 
 * @param fmt 
 * @param ... 
 */
static void cbPrintf(char * fmt, ...)
{
  va_list args;
  char str[MAX_SHELL_STRING+1];

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

// Loop-back serial device strings.
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

  char c = ' ';
  while (c != 0x1B)
  {
    // Get the next character in.
    c = loopback_getc(fp);

    // Process.
    chell_ProcessChar(c, cb);
  }

  loopback_close(fp);

  puts("\nDone.");
  return 0;
}
#endif  // STAND_ALONE