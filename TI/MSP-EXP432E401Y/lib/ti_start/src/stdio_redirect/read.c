
#include <stdio.h>
#include "uartstdio.h"

int __attribute__((weak)) _read(int file, char *ptr, int len); /* Remove GCC compiler warning */

int __attribute__((weak)) _read(int file, char *ptr, int len)
{
	int n = 0;

	if (file != 0) {
		return -1;
	}

	n = UARTgets(ptr, len);
	if (n < 0) {
		return -1;
	}

	return n;
}
