#ifndef HEX_GENERAL
#define HEX_GENERAL

#include <stdio.h>

struct file {
	char *name;           // File name
	FILE *pointer;        // File descriptor
	unsigned long size;   // File size
};

#endif
