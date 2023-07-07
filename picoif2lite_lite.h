
// include the ROM header files here
#include "rominc/romexplorer.h"   // ** do not remove this one unless you replace the ROM Exlorer utility **
#include "rominc/ROM_Tester_ROM.h"
#include "rominc/lg.h"
#include "rominc/DiagROM.h"
#include "rominc/Sinclair ZX Spectrum Test ROM (1983)(Logan, Ian)(16K)[aka Sinclair ZX Spectrum Test Cartridge].h"
#include "rominc/testrom.h"
#include "rominc/RAM_Tester_ROM.h"
#include "rominc/128_ROM.h"
#include "rominc/128_IF1_ED2_ROM.h"
#include "rominc/SS128_ROM.h"
#include "rominc/48.h"

// and put them in the order you want them to appear in the selector here
    const uint8_t *roms[] = {romexplorer                        //  0 - 4672bytes
                            ,rom_tester_rom                     //  1 - 1940bytes
                            ,lg                                 //  2 - 15558bytes
                            ,diagrom                            //  3 - 14128bytes
                            ,testrom                            //  4 - 12214bytes
                            ,sinclair_zx_spectrum_test_rom__1   //  5 - 5192bytes
                            ,ram_tester_rom                     //  6 - 3319bytes
                            ,_128_rom                           //  7 - 30339bytes
                            ,_128_if1_ed2_rom                   //  8 - 37877bytes
                            ,ss128_rom                          //  9 - 30347bytes
                            ,_48                                // 10 - 14481bytes
                            };      
