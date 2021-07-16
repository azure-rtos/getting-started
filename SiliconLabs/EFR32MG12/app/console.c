#include "sl_iostream.h"

int _read(int file, char* ptr, int len);
int _write(int file, const char* ptr, int len);

/*****************************************************************************
 * Read from a file.
 *****************************************************************************/
int _read(int file, char* ptr, int len)
{
    size_t bytes_read = 0;

    (void)file;
    sl_iostream_read(SL_IOSTREAM_STDIN, ptr, (size_t)len, &bytes_read);

    if (bytes_read == 0)
    {
        return -1;
    }

    return (int)bytes_read;
}

/*****************************************************************************
 * Write to a file.
 *****************************************************************************/
int _write(int file, const char* ptr, int len)
{
    (void)file;
    sl_iostream_write(SL_IOSTREAM_STDOUT, ptr, (size_t)len);

    return len;
}
