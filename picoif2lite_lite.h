// MAXROMs
#define MAXROMS 11
// the ROMs 
    const uint8_t *roms[] = {romexplorer            // 0 - 4609bytes ** do not change unless you replace the ROM Exlorer utility **
                            ,rom_tester_rom         // 1 - 1906bytes
                            ,lookglass              // 2 - 14094bytes
                            ,diagv59                // 3 - 12180bytes
                            ,testrom                // 4 - 5158bytes
                            ,origtest               // 5 - 3285bytes
                            ,_128kram               // 6 - 15524bytes
                            ,_128_rom               // 7 - 30305bytes
                            ,_128_if1_ed2_rom       // 8 - 37843bytes
                            ,ss128_rom              // 9 - 30313bytes
                            ,original               // 10 - 14447bytes
                            };      
// does the ROM work with ZXC compatibility?
//   this switches on reading of the top 64 bytes of ROM
//   15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
//    0  0  1  1  1  1  1  1  1  1  l  p  b  b  b  b
//     (l)ock - set to 1 to prevent further paging (0x3fe0)
//     (p)age out - set to 1 to page out the ROM cartridge, 0 to page back in (0x3fd0)
//     (b)ank - select between the 16 banks (0x3fc0)                           
    const uint8_t compatMode[] = {0
                                 ,1
                                 ,0
                                 ,0
                                 ,1
                                 ,0
                                 ,0
                                 ,1
                                 ,1
                                 ,1
                                 ,0
                                 };                                
// description to show on the screen for each ROM    
//   ** this is specific to the ROM Explorer ROM **             
// max 32 chars                  12345678901234567890123456789012
    const uint8_t *romName[] = {"Turn ZX PicoIF2Lite Off"
                               ,"ROM Tester"
                               ,"Looking Glass ROM"
                               ,"Retroleum DiagROM v1.59"
                               ,"ZX Spectrum Diagnositcs v0.37"
                               ,"ZX Spectrum Test Cartridge"
                               ,"128k RAM Tester"
                               ,"Spectrum 128k Emulator"
                               ,"Spectrum 128k Emulator with IF1"
                               ,"Spanish 128k Emulator (Espanol)"
                               ,"Original 48k ROM"
                               };    
