/**
 * @file chell.c
 * @author Tom Gordon
 * @brief Contains processChar(), which accepts a character from stdin and returns a character for stdout (or uses a call-back).
 * Provides a full-featured, Linux-style shell interface, including handling of backspace, function and arrow keys for line navigation and line recall.
 * Allows registering of shell commands, including progressive help and function handling for each. Each handler will receive the command string,
 * Number of parameters, and parameter list. Automatically provide a "help" shell command, based on the help provided for each command registered.
 * 
 * FUTURE: also provide a call-back to provide parameter hints.
 * FUTURE: also provide a "history" command handler by default.
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

// DEBUG - use in place of return call to trace processEscapes(), below.
#define traceReturn(rcode, escBuf) \
{ \printf("escapeChars: {%02X, %02X, %02X, %02X, %02X, %02X}, returning %d\n", escBuf[0], escBuf[1], escBuf[2], escBuf[3], escBuf[4], escBuf[5], rcode); \
  return rcode; \
}

/**
 * @brief This enumerates return value from processEscapes().
 * 
 */
enum
{
  ESC_UNHANDLED = -2,   // escape sequence started but no match to handled, so ignore last character 
  ESC_PROCESSING = -1,  // wait for it.. still processing escape sequence, so don't do anything yet
  ESC_NO_ACTION = 0,    // not in an escape sequence, or failed to complete an escape sequence, caller should treat as regular character
  ESC_UP_ARROW = 1,     // these must follow in order with kEscapes[] list, declared in processEscapes()
  ESC_DOWN_ARROW,
  ESC_RIGHT_ARROW,
  ESC_LEFT_ARROW,
  ESC_F1,
  ESC_F2,
  ESC_F3,
  ESC_F4,
  ESC_TAB = 9,  // not actually an escape sequence, but overlaps our enumeration
  ESC_F5,
  ESC_F6,
  ESC_F7,
  ESC_F8,
  ESC_F9,
  ESC_DEL,
  ESC_HOME,
  ESC_END,
};

/**
 * @brief Structure that defines each shell command handler, including function pointer and help information.
 * 
 */
typedef struct
{
    const char * cmd;
    void (*handler)(char const * const cmd, char const * const * param, int numParams);
    char const * help;
    // future: list of string pointers, where strings are either space-separated parameter hints (e.g. "left middle right") or ghost hints
    // as to the variable type expected (e.g. "<x coord>")
    char const * const * hints;
} Cmd_struct;

#ifndef NUM_ELEMENTS
    #define NUM_ELEMENTS(array) (sizeof(array)/sizeof(array[0]))  ///< number of elements in array of objects
#endif

// Local prototypes.
static void clearLine(void);
static bool editLine(int extChar);
static bool removeCharAtIndex(char * line, int idx);
static bool insertCharAtIndex(char * line, int idx, char c, int lineSize);
static bool processLine(char * line);
static int processEscapes(char c);
static void outChar(const char c);
static bool isEmptyLine(const char * line);
static int uniquePartialMatch(const char * str);
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
static char lineBuf[MAX_TOTAL_COMMAND_CHARS+1] = { 0 };   ///< line buffer
static int lineIdx = 0;                                   ///< next index in line buffer - also the count of characters in the line
static int editIdx = -1;                        ///< current edit index in line - -1 if not yet established for line
static int lastChar = -1;                       ///< previous character editted in line - -1 if none for line
static char histBuf[MAX_HISTORY][MAX_TOTAL_COMMAND_CHARS+1] = { 0 };  ///< command history, as circular string buffer
static int histIdx = 0;                                               ///< next index to fill in circular history string buffer

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
 * @return char* ASCIIZ string received, or empty string if complete string and line-end not received yet
 */
char * chell_ProcessChar(const char c, int (*cb_out)(int c))
{
  const char kUpArrowEscape[] = "\x1B\x5b\x41";
  const char kDownArrowEscape[] = "\x1B\x5b\x42";
  const char kRightArrowEscape[] = "\x1B\x5b\x43";
  const char kLeftArrowEscape[] = "\x1B\x5b\x44";
  const char kF3Escape[] = "\x1B\x4F\x52";
  static char escapeChars[3] = { 0 };
  static int recallIdx = -1;
  int i;
  int len;
  int extChar = 0;

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

  // Handle ctl-C.
  if (c == '\03')
  {
    // Output "^C", advance line and show prompt.
    chell_printf("^C%s%s", g_outLineEnd, g_prompt);

    // Reset line buffer and edit statics.
    lineBuf[0] = '\0';
    lineIdx = 0;
    editIdx = -1;
    lastChar = -1;
  }

  // Catch escape sequences in incoming character stream.
  int esc = processEscapes(c);
  switch(esc)
  {
    case ESC_PROCESSING:    // still processing an escape sequence - nothing more to do
      return "";
    case ESC_NO_ACTION:     // no escape sequence (and not working on one) - just process the character given
      extChar = c;
      break;
    default:    // escape sequence matched, returning a non-printabbloe integer {1,2,..} - let line editor call handle it below
      extChar = esc;
      break;
    case ESC_UP_ARROW:
      // If first time since latest line, start with latest line,
      // otherwise continue to rewind through circular history buffer.
      i = recallIdx;
      if (i < 0)
        i = (MAX_HISTORY + histIdx - 1) % MAX_HISTORY;
      else
        i = (MAX_HISTORY + recallIdx - 1) % MAX_HISTORY;

      // Nothing to do if no prior history.
      if (histBuf[i][0] == '\0')
        return "";

      // If line to recall, recall it.
      recallIdx = i;
      clearLine();  // clear current
      strcpy(lineBuf, histBuf[recallIdx]);  // set to recalled history
      lineIdx = strlen(lineBuf);    // set index to length of string recalled
      chell_printf(lineBuf);  // output back to stdout stream, via call-back
      return lineBuf; // return line recalled
    case ESC_DOWN_ARROW:
      // Nothing to wind forward to, if we haven't recalled any history yet,
      // otherwise, wind forward, stopping just short of current history index.
      i = recallIdx;
      if (i < 0)
        return "";
      else
        i = (recallIdx + 1) % MAX_HISTORY;

      // Nothing to do if we're up to current history index.
      if (i == histIdx)
        return "";

      // If line to recall, recall it.
      recallIdx = i;
      clearLine();  // clear current
      strcpy(lineBuf, histBuf[recallIdx]);  // set to recalled history
      lineIdx = strlen(lineBuf);    // set index to length of string recalled
      chell_printf(lineBuf);  // output back to stdout stream, via call-back
      return lineBuf; // return line recalled
  }

  // Edit line
  if (editLine(extChar))
  {
    /** Line-end received, command has been entered. **/

    // Terminate buffer and reset the line index.
    lineBuf[lineIdx] = '\0';  // make sure we're terminated
    lineIdx = 0;          // reset for next command

    // Reset the recall index.
    recallIdx = -1;

    if (!isEmptyLine(lineBuf))
    {
      // Put a line between what was just entered and whatever output the response will be, unless CR only entered.
      chell_printf(g_outLineEnd);

      // Save a copy of full line, before splitting into command and parameters.
      char fullLine[MAX_TOTAL_COMMAND_CHARS+1];
      strcpy(fullLine, lineBuf);

      // Parse and process string received.
      processLine(lineBuf);

      // Track history, including unhandled commands, as a circular ring buffer.
      memcpy(histBuf[histIdx], fullLine, sizeof(histBuf[0]));
      histIdx = (histIdx + 1) % MAX_HISTORY;
    }

    // Prompt
    chell_printf("%s%s", g_outLineEnd, g_prompt);
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
    chell_printf("*** You only gave me %d parameter%s, I need at least %d ***\n", numGivenParams, (numGivenParams>1)?"s":"", numExpectedParams);
    return false;
  }

  return true;
}

/**
 * @brief Clear the line buffer, and also clear using stdout stream call-back, if available.
 * 
 */
static void clearLine(void)
{
  // Wipe stream output line using call-back.
  int i;
  for (i=0;i<lineIdx;i++)
    outChar(0x08);    // back to start of line
  for (i=0;i<lineIdx;i++)
    outChar(' ');     // clear line
  for (i=0;i<lineIdx;i++)
    outChar(0x08);    // back to start of line again

  // Clear the line buffer and reset the index
  memset(lineBuf, '\0', sizeof(lineBuf));
  lineIdx = 0;
}

/**
 * @brief Edit current line, handling c/r, l/f, backspace, IDs for delete, left and right arrows,
 * and standard characters. Automatically handles either c/r or l/f single-character line-ends OR
 * c/r-l/f (or l/f-c/r) two-character line-ends.
 * This does not handle line history (up/down arrow), just single-line edits.
 * 
 * @param extChar extended character, including printables as well as escape sequence IDs {1,2..}, c/r, l/f, backspace and delete
 * @return true if line has just been completed by line-end character
 */
static bool editLine(int extChar)
{
  bool rcode = false;
  int i;

  // If first edit of line, set edit index to end of line.
  // If new line, make sure line buffer is empty.
  if (editIdx < 0)
  {
    editIdx = lineIdx;
    if (lineIdx == 0)
      lineBuf[0] = '\0';
  }

  // Handle backspace character (0x08) or single-character del (0x7F).
  if ((extChar == 0x7F) || (extChar == 0x08))
  {
    if (editIdx > 0)
    {
      // Adjust line buffer.
      editIdx--;
      removeCharAtIndex(lineBuf, editIdx);

      // Adjust output to terminal.
      chell_printf("\x08%s \x08", &lineBuf[editIdx]);   // for output, back-track and rewrite everything after the backspace
      lineIdx--;  // adjust to indicate the shorter line
      for (i=editIdx;i<lineIdx;i++)
        outChar('\x08');  // move output cursor from end of line back to back-spaced location
    }
  }

  // Handle delete.
  else if (extChar == ESC_DEL)
  {
    // Adjust line buffer.
    if (removeCharAtIndex(lineBuf, editIdx))
    {
      // Adjust output to terminal.
      chell_printf("%s ", &lineBuf[editIdx]);
      for (i=editIdx;i<lineIdx;i++)
        outChar('\x08');  // move output cursor from end of line back to back-spaced location
      lineIdx--;  // adjust to indicate the shorter line
    }
  }

  // Handle left arrow.
  else if ((extChar == ESC_LEFT_ARROW) && (editIdx > 0))
  {
    outChar('\x08');
    editIdx--;
  }

  // Handle right arrow.
  else if ((extChar == ESC_RIGHT_ARROW) && (editIdx < lineIdx))
  {
    outChar(lineBuf[editIdx]);
    editIdx++;
  }

  // Handle home key.
  else if (extChar == ESC_HOME)
  {
    while(editIdx > 0)
    {
      outChar('\x08');
      editIdx--;
    }
  }

  // Handle end key.
  else if (extChar == ESC_END)
  {
    while (editIdx < lineIdx)
    {
      outChar(lineBuf[editIdx]);
      editIdx++;
    }
  }

  // Handle tab, if editing at end of line.
  else if ((extChar == ESC_TAB) && (editIdx == lineIdx))
  {
    int idx = uniquePartialMatch(lineBuf);
    if (idx >= 0)
    {
      // Characters given so far uniquely identify a known command - complete it.
      int len = strlen(g_cmd[idx].cmd);
      while(editIdx < len)
      {
        lineBuf[lineIdx] = g_cmd[idx].cmd[lineIdx];
        outChar(lineBuf[editIdx]);
        editIdx++;
        lineIdx++;
      }
    }
  }

  // (FUTURE) handle ctrl-right, ctrl-left to skip words

  // Handle standard (printable) characters.
  else if ((extChar >= ' ') && (extChar <= '~'))
  {
    // Append or insert character in line buffer, depending on location of edit index.
    if (insertCharAtIndex(lineBuf, editIdx, extChar, sizeof(lineBuf)))
    {
      // Adjust output to terminal.
      outChar(extChar);
      editIdx++;
      chell_printf("%s", &lineBuf[editIdx]);
      lineIdx++;  // adjust to indicate longer line
      for (i=editIdx;i<lineIdx;i++)
        outChar('\x08');  // move output cursor from end of line back to back-spaced location
    }
  }
  
  // Handle c/r.
  else if (extChar == '\r')
  {
    // c/r indicates line complete UNLESS it immediately follows a l/f, in which case we
    // assume that we're being sent two-character line-ends (l/f-c/r) (is that even a thing?).
    rcode = lastChar != '\n';
  }

  // Handle l/f.
  else if (extChar == '\n')
  {
    // l/f indicates line complete UNLESS it immediately follows a c/r, in which case we
    // assume that we're being sent two-character line-ends (c/r-l/f).
    rcode = lastChar != '\r';
  }

  // Ignore anything else.
  else
  {
    return false;
  }

  // Track latest character, so we can differentiate c/r-l/f line ends from an extra blank line.
  lastChar = extChar;

  // Reset edit index on end of line.
  if (rcode)
  {
    editIdx = -1;
    lastChar = -1;
  }

  // Return that line has not yet been ended.
  return rcode;
}

/**
 * @brief Remove character at index in line, shrinking line accordingly.
 * Only allows removing at a valid character index in the line.
 * 
 * @param line ASCIIZ line string
 * @param idx index of character to remove
 */
static bool removeCharAtIndex(char * line, int idx)
{
  int len = strlen(line);

  // Return failure to shrink line if index invalid or at length of line.
  if ((idx < 0) || (idx >= len))
    return false;

  int i;
  for (i=idx;i<len;i++)
    line[i] = line[i+1];

  return true;
}

/**
 * @brief Insert character before index in line, extending line accordingly.
 * Only allows inserting from start of line (prepend) to end of line (append).
 * Will not allow line to grow beyond line buffer size given.
 * 
 * @param line ASCIIZ line string
 * @param idx index of character before which to add the given character
 * @param c character to insert
 * @param lineSize size of line buffer, including one for null-terminator (sizeof(line))
 * @return true if character successfully inserted in the line buffer
 */
static bool insertCharAtIndex(char * line, int idx, char c, int lineSize)
{
  int len = strlen(line);

  // Fail-safe: index must be within, or at end, of line.
  if ((idx < 0) || (idx > len))
    return false;

  // Fail-safe: can not insert a character into a line already at maximum length.
  if ((len+1) >= lineSize)
    return false;

  int i;
  for (i=len;i>=idx;i--)
    line[i+1] = line[i];
  line[idx] = c;

  // Successful insert
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
 * @return true if command recognized and handled
 */
static bool processLine(char * line)
{
    char * p;
    char * cmd;                             // command string (first parsed string)
    char const * param[MAX_PARAMETERS];     // list of parameters parsed for command line
    int numParams = 0;

    // Nothing to parse if empty string, or contains only line-end characters.
    if ((line == NULL) || (line[0] == '\0'))
      return false;   // return no command handled

    // Split the copied string into command and parameters by inserting
    // a NULL for each token, and assigning pointer to each. Note this does not
    // add to the line passed, only replaces the token characters with NULL.
    p = strtok(line, " ,");
    cmd = p;
    int i;
    for (i=0;i<NUM_ELEMENTS(param) && p!=NULL;i++)
    {
      p = strtok(NULL, " ,");
      if (p != NULL)
      {
        param[i] = p;
        numParams++;
      }
    }

    // Loook for match in g_cmd table, and call handler if found.
    for (i=0;i<NUM_ELEMENTS(g_cmd);i++)
    {
      if ((g_cmd[i].cmd != NULL) && (strcmp(cmd, g_cmd[i].cmd) == 0))
      {
        g_cmd[i].handler(cmd, param, numParams);
        return true;
      }
    }

    // If not handled above.
    handle_unhandled(cmd, param, numParams);
    return false;   // return no command handled
}

/**
 * @brief Look for escape sequences: known strings starting with an escape character (0x1B).
 * These include up, down, left and right arrow keys, delete, and various function keys.
 * The return value indicates the state of gathering an escape sequence:
 *   -1 : processing a sequence, not complete : caller should discard the incoming character
 *    0 : no escape seuence is being gathered : caller should process the character as normal
 *   1+ : an escape sequence just recognized : caller should take action based on the return value, ignoring incoming character
 * Note thise routine does not validate the character, it only filters and reports on known escape sequences. Caller must insure
 * that any character not handled here is valid for other purposes.
 * Note that partial match may cause some incoming characters to be lost.
 * 
 * @param c next character to process
 * @return int 0 if not collecting escape characters, -1 if in process of collecting an escape sequence, or {1,2..} if recognized escape sequence
 */
static int processEscapes(char c)
{
// TODO: need to handle any unrecognized escape sequences gracefully - still filter out characters that belong to ones we don't handle here!
  // This list must match 1-based enumeration defined above.
  // Since all strings are null-terminated, we can determine the number of used characters by looking for the null termination. Strings shorter
  // than the alloted array size for each entry must padd with null(s).
  const char kEscapes[][6] =
  {
    "\x1B\x5B\x41\x00\x00",     // up arrow
    "\x1B\x5B\x42\x00\x00",     // down arrow
    "\x1B\x5B\x43\x00\x00",     // right arrow
    "\x1B\x5B\x44\x00\x00",     // left arrow
    "\x1B\x4F\x50\x00\x00",     // F1
    "\x1B\x4F\x51\x00\x00",     // F2
    "\x1B\x4F\x52\x00\x00",     // F3
    "\x1B\x4F\x53\x00\x00",     // F4
    "\x1B\x5B\x31\x35\x7E",     // F5
    "\x1B\x5B\x31\x37\x7E",     // F6
    "\x1B\x5B\x31\x38\x7E",     // F7
    "\x1B\x5B\x31\x39\x7E",     // F8
    "\x1B\x5B\x32\x30\x7E",     // F9
    "\x1B\x5B\x33\x7E\x00",     // delete
    "\x1B\x5B\x31\x7E\x00",     // home
    "\x1B\x5B\x34\x7E\x00",     // end
  };
  static char escapeChars[sizeof(kEscapes[0])] = { 0 };

  // Establish the next index in the escape sequence, knowing that the array
  // will be cleared with each new escape character (0x1B).
  int escIdx;
  for (escIdx=0;escIdx < sizeof(escapeChars);escIdx++)
    if (escapeChars[escIdx] == 0)
      break;
  
  if (c == '\x1B')
  {
    // In all cases, an escape character aborts any previous sequence, so clear the escape sequence buffer.
    memset(escapeChars, 0, sizeof(escapeChars));

    if ((escIdx == 1) && (escapeChars[0] == '\x1B'))
    {
      // If two escapes in a row (one already buffered), tell caller to process escape as a regular character,
      // and ignore it here.
      return ESC_NO_ACTION;
      // traceReturn(ESC_NO_ACTION, escapeChars)
    } else
    {
      // Otherwise start buffer with it here, and tell caller that we're currently gathering a possible escape sequence.
      escapeChars[0] = c;
      return ESC_PROCESSING;
      // traceReturn(ESC_PROCESSING, escapeChars)
    }
  }

  // If not an escape character (above), and no escape sequence started, then we take no action, caller should handle as regular character.
  if (escIdx == 0)
    return ESC_NO_ACTION;
    // traceReturn(ESC_NO_ACTION, escapeChars)

  // Buffer incoming. Fail-safe: insure index never exceeds buffer.
  if (escIdx < (sizeof(escapeChars)-1))
    escapeChars[escIdx++] = c;

  // Scan all known sequences for a match.
  int eseqIdx;
  for (eseqIdx=0;eseqIdx<NUM_ELEMENTS(kEscapes);eseqIdx++)    
  {
    int i;
    for (i=0;i<escIdx;i++)
    {
      // If match so far..
      if (escapeChars[i] == kEscapes[eseqIdx][i])
      {
        if (kEscapes[eseqIdx][i+1] == '\x00')
        {
          // If last before termination, we have a match - return 1-based index.
          memset(escapeChars, 0, sizeof(escapeChars));
          return eseqIdx + 1;
          // traceReturn(eseqIdx + 1, escapeChars)
        }
      } else
      {
        // Mismatch, try next.
        break;
      }
    }

    // All characters match so far, but more to check - return still processing.
    if (i >= escIdx)
      return ESC_PROCESSING;
      // traceReturn(ESC_PROCESSING, escapeChars)
  }

  // If failed to match any known sequence, reset and tell caller to ignore the latest character, assuming it failed on
  // the last character - if there are more to a valid escape sequence that we don't recognize, one or more
  // characters from the end of that sequence my be passed on as regular characters.
  if (eseqIdx >= NUM_ELEMENTS(kEscapes))
  {
    memset(escapeChars, 0, sizeof(escapeChars));
    return ESC_UNHANDLED;
    // traceReturn(ESC_UNHANDLED, escapeChars)
  } else
  {
    // Otherwise waiting on more characters to match - return "still processing".
    return ESC_PROCESSING;
    // traceReturn(ESC_PROCESSING, escapeChars)
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
 * @brief Look for a partial (or full) match of the given string to exactly
 * one registered handler command. (FUTURE - also do constant string parameters?)
 * 
 * @param str partial line to match
 * @return index of handler structure whose command string at least partially matches, -1 if no or multiple matches
 */
static int uniquePartialMatch(const char * str)
{
  int len = strlen(str);
  int midx = -1;
  int i;
  for (i=0;i<g_numRegCmds;i++)
  {
    if (strncmp(str, g_cmd[i].cmd, len) == 0)
    {
      if (midx != -1)
        return -1;  // multiple matches
      midx = i;
    }
  }

  // Return index found, or -1 if none.
  return midx;
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
#include <fcntl.h>

#include "loopback.h" // for testing with a loop-back connection

// Loop-back serial device strings. This is for stand-alone testing. The caller
// into this library is expected to both provide characters from an incoming stream
// and provide a call-back to allow returning characters to an outgoing stream. This library
// does not do streams, it just processes incoming characters and parses and processes commands.
#ifdef __linux__
char const * const devstr = "/dev/pts/5";
#else
char const * const devstr = "COM6";
#endif // __linux__

int fd = -1;

// Use loopback out as our output call-back.
int cb(int c) { loopback_put(c, fd); }

/**
 * @brief Main entry function, for testing via command-line.
 */
int main(int argc, char * argv[])
{
  fd = loopback_open(devstr);
  if (fd < 0)
  {
    printf("Failed to open \"%s\"\n", devstr);
    return -1;
  }

  // // DEBUG: test processEscapes.
  // while (true)
  // {
  //   char c = loopback_getc(fp);
  //   int rcode = processEscapes(c);
  //   if (rcode == ESC_NO_ACTION)
  //     printf("%c", c);
  //   else if (rcode == ESC_UNHANDLED)
  //     continue;
  //   else if (rcode >= 1)
  //     printf("Escape %d\n", rcode);
  // }

  // // DEBUG: show key codes and escape sequences.
  // while(true)
  // {
  //   char c = loopback_get(fd);
  //   if (c == '\r')
  //   {
  //     printf("\n");
  //     continue;
  //   }
  //   printf("%02X", c);
  //   if ((c >= ' ') && (c <= '~'))
  //     printf("(%c)", c);
  //   puts("");
  // }

  printf("Using serial I/O through \"%s\" fd=%d\n", devstr, fd);

  // Example use of registering handler and setting prompt.
  chell_RegisterHandler("add", handle_example, "add two numbers", NULL);
  chell_setPrompt("> ");

  char c = ' ';
  while (c != '*')
  {
    // Get the next character in.
    c = loopback_get(fd);

    // Process.
    chell_ProcessChar(c, cb);
  }

  loopback_close(fd);

  chell_printf(g_outLineEnd);
  return 0;
}
#endif  // STAND_ALONE