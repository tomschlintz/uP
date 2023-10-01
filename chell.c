/**
 * @file chell.c
 * @author Tom Gordon
 * @brief Contains processChar(), which accepts a character from stdin and returns a character for stdout (or uses a call-back).
 * Provides a full-featured shell interface, including handling of backspace, function and arrow keys for line navigation and line recall.
 * Allows registering of shell commands, including progressive help and function handling for each. Each handler will receive the command string,
 * Number of parameters, and parameter list. Automatically provide a "help" shell command, based on the help provided for each command registered.
 * May also provide a call-back to provide auto-fill suggestions.
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

// File globals.
static bool g_helpInitialized = false;    // help command list has been initialized, to include help handler
static Cmd_struct g_cmd[MAX_COMMANDS] = { 0 };   // list of commands, as registered
static int g_numRegCmds = 0;  // the number of commands registered, including the standard help

static void handle_help(char const * const cmd, char const * const * param, int numParams)
{
  (void)cmd;
  (void)param;
  (void)numParams;

  printf("Built %s %s. Commands:", __DATE__, __TIME__);
  int i;
  for (i=0;i<g_numRegCmds; i++)
      printf("\t%s %s\n", g_cmd[i].cmd, g_cmd[i].help);
}

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
 * @brief Call from main loop as often as possible, looks for incoming serial characters, echos, 
 * and allows for backspace to retype and escape to start over. Returns NULL until c/r received.
 * 0x1B 0x5B 0x41 escape sequence = up arrow
 * 0x1B 0x4F 0x52 escape sequence = F3
 * 
 * @param c ASCIIZ character that is next in stream of characters to process
 * @param cb_out call-back to stdout stream
 * @return char* ASCIIZ string received, or NULL if complete string and line-end not received yet
 */
char * chell_ProcessChar(const char c, void (*cb_out)(const char c))
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

  // If chell_RegisterHandler() was never called to register any user commands, then initialize it now so that at least "help" is handled.
  // Maybe make it a special help command, to provide help on how to register commands ?
  if (!g_helpInitialized)
  {
    g_helpInitialized = true;
  }

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
        buf[MAX_TOTAL_COMMAND_CHARS] = '\0';
        idx = strlen(buf);
        int i;
// TODO: need some mechanism for echoing characters to system-defined outgoing stream.
        // for (i=0;i<MAX_CMD;i++)
        //   cb_out(0x08);
        // for (i=0;i<MAX_CMD;i++)
        //   cb_out(' ');
        // for (i=0;i<MAX_CMD;i++)
        //   cb_out(0x08);
        // for (i=0;i<idx;i++)
        //   cb_out(buf[i]);
      }

      if ((memcmp(escapeChars, kLeftArrowEscape, sizeof(escapeChars)) == 0) && (idx > 0))
      {
        // For left arrow, move cursor and index left, to start of string.
        cb_out(0x08);
        idx--;
      }

      if ((memcmp(escapeChars, kRightArrowEscape, sizeof(escapeChars)) == 0) && (idx < (MAX_TOTAL_COMMAND_CHARS)) && (buf[idx] != '\0'))
      {
        // For right arrow, move cursor and index right, to just past end of string currently buffered.
        cb_out(0x08);
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
      cb_out(0x08);
      cb_out(' ');
      cb_out(0x08);
      idx--;
    }
  } else if ((c == 0x0D) || (c == 0x0A))
  {
    // // Flush receive buffer to clear out any remaining line-end characters.
    // while (Serial.available() > 0)
    //   Serial.read();

    // Reset and return buffer on c/r or l/f.
    buf[idx] = '\0';  // make sure we're terminated
    idx = 0;          // reset for next command
    cb_out('\n'); // echo the line-end
    memcpy(history[hidx], buf, sizeof(history[0]));
    hidx = (hidx + 1) % MAX_HISTORY;  //TODO: always place latest line at end of history
    return buf;
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

  // No c/r yet.
  return NULL;
}

#ifdef STAND_ALONE
/**
 * @brief Main entry function, for testing via command-line.
 */
int main(int argc, char * argv[])
{
  // Register set of test commands.
  // Accept and process input, echo output, until escape key hit.
  char c = '\0';
  while (c != 0x1B)
  {
    c = getchar();
    putchar(c);
  }
}
#endif  // STAND_ALONE