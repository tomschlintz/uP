#ifndef CHELL_H
#define CHELL_H

// Defines and macros.
#define MAX_SHELL_STRING 255    ///< maximum single command-line string, including parameters, we can handle (sizes buffers)

// Prototypes.
bool chell_RegisterHandler(const char * cmd, void (*handler)(char const * const cmd, char const * const * param, int numParams), const char * help, char const * const * hints);
char * chell_ProcessChar(const char c, int (*cb_out)(int c));
void chell_setOutLineEnd(const char * str);

#endif  // CHELL_H