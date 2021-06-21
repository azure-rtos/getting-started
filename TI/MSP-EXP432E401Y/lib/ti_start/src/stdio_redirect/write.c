
#include <stdio.h>
#include "uartstdio.h"

int __attribute__((weak)) _write(int file, char *ptr, int len); /* Remove GCC compiler warning */

int __attribute__((weak)) _write(int file, char *ptr, int len)
{
	int n = 0;

	if ((file != 1) && (file != 2) && (file != 3)) {
		return -1;
	}

	n = UARTwrite(ptr, len);
	if (n < 0) {
		return -1;
	}

	return n;
}
