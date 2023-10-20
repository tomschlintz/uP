#ifndef LOOPBACK_H
#define LOOPBACK_H

#include <stdio.h>

// defines
// prototypes
FILE * loopback_open(const char * devStr);
void loopback_close(FILE * fp);
char loopback_getc(FILE * fp);
void loopback_putc(char c, FILE * fp);

#endif // LOOPBACK_H