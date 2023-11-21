#ifndef COMMS_H
#define COMMS_H

// prototypes
int comms_open(const char * devStr);
void comms_close(int fd);
char comms_get(int fd);
void comms_put(char c, int fd);

#endif // COMMS_H