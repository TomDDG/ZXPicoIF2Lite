#ifndef ROM_DESCRIPTOR_H
#define ROM_DESCRIPTOR_H

#include <stdio.h>

typedef enum CompatMode {
    BUILT_IN_ROM = 0x0,
    ROM_CARTRIDGE = 0x1,
    SNAPSHOT_48K = 0x3,
    SNAPSHOT_128K = 0x8
} CompatMode;

typedef struct RomDescriptor {

    /*
     * Description to show on the screen for each ROM.
     * NOTE: this is specific to the ROM Explorer ROM.
     * max 32 characters
     */
    const char *romName;
    const uint32_t romSize;
    /*
     * Does the ROM work with ZXC compatibility?
     * this switches on reading of the top 64 bytes of ROM
     * 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
     *  0  0  1  1  1  1  1  1  1  1  l  p  b  b  b  b
     * (l)ock - set to 1 to prevent further paging (0x3fe0)
     * (p)age out - set to 1 to page out the ROM cartridge, 0 to page back in (0x3fd0)
     * (b)ank - select between the 16 banks (0x3fc0)
     */
    const CompatMode compatMode;
    const uint8_t *romCode;
    int16_t position;
    struct RomDescriptor *next;

} RomDescriptor;

extern RomDescriptor *firstRomDescriptor;
extern RomDescriptor *lastRomDescriptor;
extern uint16_t maxRoms;
extern RomDescriptor **romLookupTable;

void registerRom(RomDescriptor *romDescriptor);

#endif // ROM_DESCRIPTOR_H
