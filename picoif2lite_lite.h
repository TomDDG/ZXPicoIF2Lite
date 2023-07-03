#include "rom_descriptor.h"

/*
 * The ROM explorer must be registered explicitly before all other ROMs.
 *
 * WARNING: DO NOT CHANGE UNLESS YOU WANT TO REPLACE THE ROM EXPLORER UTILITY.
 */
#include "roms/rom_explorer.h"

RomDescriptor liteRomDescriptor[] = {
        {
                "ROM Tester",
                1906,
                1,
                rom_tester_rom,
                1,
                NULL
        },
        {
                "Looking Glass ROM",
                14094,
                0,
                lookglass,
                2,
                NULL
        },
        {
                "Retroleum DiagROM v1.59",
                12180,
                0,
                diagv59,
                3,
                NULL
        },
        {
                "ZX Spectrum Diagnostics v0.37",
                5158,
                1,
                testrom,
                4,
                NULL
        },
        {
                "ZX Spectrum Test Cartridge",
                3285,
                0,
                origtest,
                5,
                NULL
        },
        {
                "128k RAM Tester",
                15524,
                0,
                _128kram,
                6,
                NULL
        },
        {
                "Spectrum 128k Emulator",
                30305,
                1,
                _128_rom,
                7,
                NULL
        },
        {
                "Spectrum 128k Emulator with IF1",
                37843,
                1,
                _128_if1_ed2_rom,
                8,
                NULL
        },
        {
                "Spanish 128k Emulator (Espanol)",
                30313,
                1,
                ss128_rom,
                9,
                NULL
        },
        {
                "Original 48k ROM",
                14447,
                0,
                original,
                10,
                NULL
        },
};

#pragma startup registerLiteRoms
void registerLiteRoms() __attribute__((constructor));

void registerLiteRoms() {
    for (int i = 0; i < (sizeof(liteRomDescriptor)/sizeof(RomDescriptor)); i++)
        registerRom(&liteRomDescriptor[i]);
}
