#include <stdlib.h>
#include "rom_descriptor.h"

RomDescriptor *firstRomDescriptor = NULL;
RomDescriptor *lastRomDescriptor = NULL;
uint16_t maxRoms = 0;
RomDescriptor **romLookupTable;

/**
 * Adds the given ROM descriptor to the internal ROM explorer's lookup table.
 *
 * @param romDescriptor  ROM descriptor containing the name of the ROM and the location of the ROM data amongst other
 * things.
 */
void registerRom(RomDescriptor *romDescriptor) {

    maxRoms++;

    if (firstRomDescriptor == NULL) {
        firstRomDescriptor = romDescriptor;
        lastRomDescriptor = firstRomDescriptor;
    } else {
        lastRomDescriptor->next = romDescriptor;
        lastRomDescriptor = romDescriptor;
    }

    free(romLookupTable);
    romLookupTable = calloc(sizeof(RomDescriptor *), maxRoms);
    RomDescriptor *src = firstRomDescriptor;
    RomDescriptor **dst = romLookupTable;
    while (src != NULL) {
        *dst = src;
        dst++;
        src = src->next;
    }
}
