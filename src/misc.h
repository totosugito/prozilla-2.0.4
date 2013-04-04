#ifndef MISC_H
#define MISC_H

#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>

int is_number(char *);

char *kstrdup(const char *);

int setargval(char *, int *);

int setlongargval(char *, long *);

void delay_ms(int);

#endif
