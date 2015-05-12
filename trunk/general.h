#ifndef HEX_GENERAL
#define HEX_GENERAL

#include <stdio.h>

#define PVER "1.0.4"

struct file {
	char *name;           /* File name       */
	FILE *pointer;        /* File descriptor */
	unsigned long size;   /* File size       */
};

#endif
