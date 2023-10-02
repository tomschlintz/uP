#ifndef CHELL_H
#define CHELL_H

// Prototypes.
bool chell_RegisterHandler(const char * cmd, void (*handler)(char const * const cmd, char const * const * param, int numParams), const char * help, char const * const * hints);
char * chell_ProcessChar(const char c, int (*cb_out)(int c));
void setOutLineEnd(char c);

#endif  // CHELL_H