#ifndef CHELL_H
#define CHELL_H

// Defines. Some or all of these may be adjusted down to save RAM, or up to accommodate larger strings or number of parameters.
#define MAX_STR 64          ///< maximum command or parameter string expected, for sizing arrays.
#define MAX_PARAMETERS 16   ///< maximum number of command and parameter strings expected - note this will multiply by MAX_STR when allocating string storage!
#define MAX_TOTAL_COMMAND_CHARS ((MAX_PARAMETERS+1) * MAX_STR + MAX_PARAMETERS)   ///< string length enough for command and parameters, including a space between each
#define MAX_HISTORY 16      ///< depth of recall history
#define MAX_COMMANDS 64     ///< maximum number of commands that may be registered
#define MAX_SHELL_PROMPT 64 ///< maximum characters allowed for prompt string

// Prototypes.
bool chell_RegisterHandler(const char * cmd, void (*handler)(char const * const cmd, char const * const * param, int numParams), const char * help, char const * const * hints);
char * chell_ProcessChar(const char c, int (*cb_out)(int c));
void chell_setOutLineEnd(const char * str);
void chell_setPrompt(const char * str);
bool chell_confirmParameters(int numGivenParams, int numExpectedParams);

#endif  // CHELL_H