#ifndef NDEBUG

#include <assert.h>
#include <stddef.h>

#include "dprintf.h"
#include "dump.h"

void dump(const void *ptr, size_t nbytes)
{
    const uint8_t *bytes;
    uint8_t c;
    size_t i;
    size_t j;

    assert(ptr != NULL || nbytes == 0);

    if (nbytes == 0) {
        dprintf("\t--- Empty ---\n");
    }

    bytes = ptr;

    for (i = 0 ; i < nbytes ; i += 16) {
        dprintf("    %08x:", (int) i);

        for (j = 0 ; i + j < nbytes && j < 16 ; j++) {
            dprintf(" %02x", bytes[i + j]);
        }

        while (j < 16) {
            dprintf("   ");
            j++;
        }

        dprintf(" ");

        for (j = 0 ; i + j < nbytes && j < 16 ; j++) {
            c = bytes[i + j];

            if (c < 0x20 || c >= 0x7F) {
                c = '.';
            }

            dprintf("%c", c);
        }

        dprintf("\n");
    }

    dprintf("\n");
}

#endif
