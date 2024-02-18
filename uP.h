#ifndef UP_H
#define UP_H

#ifdef __cplusplus
extern "C" {
#endif

#define UP_VERSION 0.02   ///< this is a pre-release version: not for distribution beyond specific projects

// Defines. Some or all of these may be adjusted down to save RAM, or up to accommodate larger strings or number of parameters.
// TODO: might be nice to comment on the impact on memory of expanding each of these.
#define MAX_STR 16          ///< maximum command or parameter string expected, for sizing arrays.
#define MAX_PARAMETERS 8    ///< maximum number of command and parameter strings expected - note this will multiply by MAX_STR when allocating string storage!
#define MAX_TOTAL_COMMAND_CHARS ((MAX_PARAMETERS+1) * MAX_STR + MAX_PARAMETERS)   ///< string length enough for command and parameters, including a space between each
#define MAX_HISTORY 16      ///< depth of recall history
#define MAX_COMMANDS 64     ///< maximum number of command handlers that may be registered
#define MAX_SHELL_PROMPT 16 ///< maximum characters allowed for prompt string

// Prototypes.
bool uP_RegisterHandler(const char * cmd, void (*handler)(char const * const cmd, char const * const * param, int numParams), const char * help, char const * const * hints);
char * uP_ProcessChar(const char c, int (*cb_out)(int c));
void uP_setOutLineEnd(const char * str);
void uP_setPrompt(const char * str);
bool uP_confirmParameters(int numGivenParams, int numExpectedParams);

#ifdef __cplusplus
} // extern "C"
#endif

#endif  // UP_H