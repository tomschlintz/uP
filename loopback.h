#ifndef LOOPBACK_H
#define LOOPBACK_H

#include <stdio.h>

// defines
// prototypes
int loopback_open(const char * devStr);
void loopback_close(int fd);
char loopback_get(int fd);
void loopback_put(char c, int fd);

#endif // LOOPBACK_H