// Versions
// v0.1 initial release taken form PicoIF2ROM but with interrupt drive user button
// v0.2 changes to improve ZXC compatibility, added zxcOn true/false
// v0.3 Z80/SNA snapshot support, zxcOn->compatMode
// v0.4 fixed issue with not loading correctly on earlier Spectrums, needed i register setting at 0x80
//
#define PROG_NAME   "ZX PicoIF2Lite"
#define VERSION_NUM "v0.4"
// ---------------------------------------------------------------------------
// includes
// ---------------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "picoif2lite.pio.h"
#include "rom_descriptor.h"
//#include "picoif2lite.h"   // header
#include "roms_lite.h"          // the ROMs (lite version)
#include "picoif2lite_lite.h"   // header (lite version)

// ---------------------------------------------------------------------------
// gpio pins
// ---------------------------------------------------------------------------
#define PIN_A0      0   // GPIO 0-13 for A0-A13
#define PIN_D0      14  // GPIO 15-21 for D0-D7
#define PIN_LED     25  // Default LED pin for Pico (not W)
#define NUM_ADDR_PINS   14
#define NUM_DATA_PINS   8

//                  3         2         1
//                 10987654321098765432109876543210
#define MASK_LED 0b00000010000000000000000000000000

//
#define PIN_RESET   28  // GPIO to control RESET of Spectrum 
#define PIN_USER    22  // User input GPIO (v1.1 PCB this is 22)
#define PIN_ROMRQ   26  // ROM Request
#define PIN_ROMCS   27  // ROMCS

#define poMask   0b0011111111010000
#define poMaskn  0b0011111111000000
#define lkMask   0b0011111111100000
#define bkMask   0b0000000000001111

#define ADDR_CURRENT_ROM    0x0009      // 0x0005 current rom ** this is specific to the ROM Explorer ROM **
#define ADDR_CURRENT_POS    0x000e      // 0x000a current pos ** this is specific to the ROM Explorer ROM **
#define ADDR_CURRENT_PAGE   0x0013      // 0x000f current page ** this is specific to the ROM Explorer ROM **
#define ADDR_MAX_ROMS       0x0123      // 0x0123 max ROMs (-1) ** this is specific to the ROM Explorer ROM **
#define ADDR_MAX_PAGES      0x0160      // 0x0123 max pages ** this is specific to the ROM Explorer ROM **

uint8_t bank1[0x20000];   // equivalent to a 128K EPROM
uint8_t romSelector[0x4000];
volatile uint8_t rompos = 0;
volatile bool pagingOn = false;
volatile uint32_t adder = 0;
PIO pio;
uint addr_data_sm;


void dtoBuffer(uint8_t *to, const uint8_t *from);

uint16_t simplelz(uint8_t *fload, uint8_t *store, uint16_t filesize);

void resetButton(uint gpio, uint32_t events);


int main() {
    // ---------------------------------------------------------------------
    // build the ROM Selector ROM from picoif2lite.h, only need to do this once
    //   ** this is specific to the ROM Explorer ROM **
    // ---------------------------------------------------------------------
    dtoBuffer(romSelector,romLookupTable[0]->romCode); // ROM Explorer ROM into romSelector
    romSelector[ADDR_MAX_ROMS] = maxRoms - 1;
    romSelector[ADDR_MAX_PAGES] = ((maxRoms - 1) / 21) + 1;
    uint16_t bpos = 0;
    for (uint16_t romNum = 0; romNum < maxRoms; romNum++) {
        bank1[bpos++] = (romNum == (maxRoms - 1)) ? 31 : 30;

        for (uint i = 0; i < strlen(romLookupTable[romNum]->romName); i++) {
            if (romLookupTable[romNum]->romName[i] < 0x20 || romLookupTable[romNum]->romName[i] >= 0x80) {
                bank1[bpos++] = 0x20;
            } else {
                bank1[bpos++] = romLookupTable[romNum]->romName[i];
            }
        }
        switch (romLookupTable[romNum]->compatMode) {
            case ROM_CARTRIDGE:
                bank1[bpos++] = 9;
                bank1[bpos++] = 28;
                bank1[bpos++] = 29;
                break;
            case SNAPSHOT_48K:
            case SNAPSHOT_128K:
                bank1[bpos++] = 9;
                bank1[bpos++] = 26;
                bank1[bpos++] = 27;
                break;
            default:
                // Compatibility mode isn't required for this ROM.
                break;
        }

        bank1[bpos++] = (romNum == (maxRoms - 1) || (romNum + 1) % 21 == 0) ? 0 : 10;
    }

    // Compress the text and put into romSelector
    simplelz(bank1,&romSelector[0x1e00],bpos);

    // -------------------------------
    // set-up USER, ROMCS & RESET gpio
    // -------------------------------
    gpio_init(PIN_RESET);
    gpio_set_dir(PIN_RESET, GPIO_OUT);
    gpio_put(PIN_RESET, false);    // hold in RESET state till ready
    //
    gpio_init(PIN_USER);
    gpio_set_dir(PIN_USER, GPIO_IN);
    gpio_pull_up(PIN_USER); // button active when connected to ground

    // When user button pressed, interrupt code and run resetButton routine
    gpio_set_irq_enabled_with_callback(PIN_USER, GPIO_IRQ_EDGE_FALL, true, &resetButton);

    gpio_init(PIN_ROMCS);
    gpio_set_dir(PIN_ROMCS, GPIO_OUT);
    gpio_put(PIN_ROMCS, false);    // start with ROM off
    //
    gpio_init(PIN_LED);
    gpio_set_dir(PIN_LED, GPIO_OUT);
    gpio_put(PIN_LED, false);

    // ----------
    // Set-up PIO
    // ----------
    pio = pio0; // use pio 0
    // grab a free state machine from PIO 0
    addr_data_sm = pio_claim_unused_sm(pio, true);
    // get instruction memory offset for the loaded program
    uint addr_data_offset = pio_add_program(pio, &picoif2_program);
    // get the default state machine config
    pio_sm_config addr_data_config = picoif2_program_get_default_config(addr_data_offset);
    // set-up IN pins
    for (uint i = PIN_A0; i < PIN_A0 + NUM_ADDR_PINS; i++) {
        pio_gpio_init(pio, i); // initialise all 14 input pins
    }
    pio_sm_set_consecutive_pindirs(pio, addr_data_sm, PIN_A0, NUM_ADDR_PINS, false); // input
    sm_config_set_in_pins(&addr_data_config, PIN_A0); // set IN pin base
    // shift left 14 pins (A13-A0) into ISR, auto-push the resultant 32bit address to DMA RXF
    sm_config_set_in_shift(&addr_data_config, false, true, NUM_ADDR_PINS);
    pio_gpio_init(pio, PIN_ROMRQ);
    pio_sm_set_consecutive_pindirs(pio, addr_data_sm, PIN_ROMRQ, 1, false); // input

    // set-up OUT data pins
    for (uint i = PIN_D0; i < PIN_D0 + NUM_DATA_PINS; i++) {
        pio_gpio_init(pio, i); // initialize all 8 output pins
    }
    // set OUT pin base & number (bits)
    sm_config_set_out_pins(&addr_data_config, PIN_D0, NUM_DATA_PINS);
    // right shift 8 bits of OSR to pins (D0-D7) with auto-pull on
    sm_config_set_out_shift(&addr_data_config, true, true, NUM_DATA_PINS);
    // set all output pins to output, D0-D7
    pio_sm_set_consecutive_pindirs(pio, addr_data_sm, PIN_D0, NUM_DATA_PINS, true);
    // reset state machine and configure it
    pio_sm_init(pio, addr_data_sm, addr_data_offset, &addr_data_config);

    // start PIO state machine
    pio_sm_set_enabled(pio, addr_data_sm, true); // enable state machine
    busy_wait_us_32(50000);       // wait 50ms before lifting RESET
    gpio_put(PIN_RESET, true);    // release RESET

    // loop forever
    while (true) {
        uint32_t address = pio_sm_get_blocking(pio, addr_data_sm);
        // If ROMCS is off, then the direction of data chip is input, so they do not interfere.
        pio_sm_put_blocking(pio, addr_data_sm, bank1[address + adder]);
        // z80 routine
        if ((romLookupTable[rompos]->compatMode == SNAPSHOT_48K ||
             romLookupTable[rompos]->compatMode == SNAPSHOT_128K)) {
            if (address == 0x3fff) {
                if (pagingOn == true) {
                    adder += 0x4000;
                    if (adder == romLookupTable[rompos]->compatMode * 0x4000) {
                        adder = 0;
                        pagingOn = false;
                    }
                } else {
                    gpio_put(PIN_LED, false);
                    gpio_put(PIN_ROMCS, false);    // ROM off
                }
            }
        } else if (romLookupTable[rompos]->compatMode == ROM_CARTRIDGE && pagingOn == true) {
            // top 64 ROM locations (0x3fc0-0x3fff)
            // 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
            //  0  0  1  1  1  1  1  1  1  1  l  p  b  b  b  b
            // (l)ock - set to 1 to prevent further paging (0x3fe0)
            // (p)age out - set to 1 to page out the ROM cartridge, 0 to page back in (0x3fd0)
            // (b)ank - select between the 16 banks
            if (address >= 0x3fc0) {
                // page in/out
                if ((address & poMask) == poMask) {
                    gpio_put(PIN_ROMCS, false);    // ROM off
                } else {
                    gpio_put(PIN_ROMCS, true);    // ROM on
                }
                // bank 0-7 (not enough memory for all 16 banks, only 8 allowed)
                adder = (address & bkMask) * 0x4000;
                if (adder > 0x20000)
                    adder = 0;
                // lock paging
                if ((address & lkMask) == lkMask) {
                    pagingOn = false;
                    gpio_put(PIN_LED, false);
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// resetButton - interrupt driven routine when user button pressed
// input:
//   gpio - which gpio called this routine
//   events - the compressed storage
// ---------------------------------------------------------------------------
void resetButton(uint gpio, uint32_t events) {
    uint32_t address;
    gpio_put(PIN_RESET, false); // put Spectrum in RESET state
    gpio_put(PIN_LED, false);
    // wait for button release and check held for 1second to switch ROM otherwise just reset
    uint64_t lastPing = time_us_64();
    do {
        busy_wait_us_32(100000); // wait 100ms between each read, minimum 100ms wait on each press
    } while ((gpio_get(PIN_USER) == false) && (time_us_64() < lastPing + 1000000));
    // button pressed for >=1second?
    if (time_us_64() >= lastPing + 1000000) {
        romSelector[ADDR_CURRENT_ROM] = rompos;
        romSelector[ADDR_CURRENT_POS] = rompos - ((rompos / 21) * 21);
        romSelector[ADDR_CURRENT_PAGE] = (rompos / 21) + 1;
        // run the Selector ROM
        gpio_put(PIN_ROMCS, true);     // turn on ROMCS
        busy_wait_us_32(50000);       // wait 50ms before lifting RESET            
        uint16_t countAddress = 0;
        gpio_put(PIN_RESET, true);   // lift reset
        do {
            address = pio_sm_get_blocking(pio, addr_data_sm);
            pio_sm_put_blocking(pio, addr_data_sm, romSelector[address]);
            if (address >= 0x3f80)
                countAddress++;
        } while (countAddress < 256);    // wait for consistent signal above 0x3f80 from ROM selector
        // ROM selected
        gpio_put(PIN_RESET, false); // put Spectrum in RESET state
        rompos = address - 0x3f80;

        if (rompos >= maxRoms) {
            rompos = maxRoms - 1; // error trap
        }
        dtoBuffer(bank1, romLookupTable[rompos]->romCode);  // unpack correct ROM
        busy_wait_us_32(200000); // wait 200ms to show rom selector screen                                            
    }

    // final set-up before restart
    if (romLookupTable[rompos]->compatMode == ROM_CARTRIDGE ||
        romLookupTable[rompos]->compatMode == SNAPSHOT_48K ||
        romLookupTable[rompos]->compatMode == SNAPSHOT_128K) {
        pagingOn = true; // if the ROM had this off make sure it is back on
        gpio_put(PIN_LED, true);
    }

    if(rompos==0) {
        gpio_put(PIN_ROMCS,false);     // turn off ROMCS      
    } else {
        gpio_put(PIN_ROMCS,true);     // turn on ROMCS  
    }    
    adder=0;
    gpio_put(PIN_RESET,true);   // lift reset    
    // // wait for 0x0000 memory access before returning
    // do {
    //     address=pio_sm_get_blocking(pio,addr_data_sm);
    //     pio_sm_put_blocking(pio,addr_data_sm,bank1[address]); 
    // } while(address!=0x0000);
}

//
// ---------------------------------------------------------------------------
// dtoBuffer - decompress compressed ROM directly into buffer (simple LZ)
// input:
//   to - the buffer
//   from - the compressed storage
// *simple LZ has a simple 256 backwards window and greedy parser but is very
// fast
// ---------------------------------------------------------------------------
void dtoBuffer(uint8_t *to,const uint8_t *from) { 
    uint i=0,j=0,k;
    uint8_t c,o;
    do {
        c=from[j++];
        if(c==128) return;
        else if(c<128) {
            for(k=0;k<c+1;k++) to[i++]=from[j++];
        }
        else {
            o=from[j++]; // offset
            for(k=0;k<(c-126);k++) {
                to[i]=to[i-(o+1)];
                i++;
            }
        }
    } while(true);
}
//
// ---------------------------------------------------------------------------
// simplelz - very simple lz with 256byte backward look
//   x=128+ then copy sequence from x-offset from next byte offset 
//   x=0-127 then copy literal x+1 times
//   minimum sequence size 2
// ---------------------------------------------------------------------------
uint16_t simplelz(uint8_t* fload,uint8_t* store,uint16_t filesize) {
	uint16_t i;
	uint8_t * store_p, * store_c;
	uint8_t litsize = 1;
	uint16_t repsize, offset, repmax, offmax;
	store_c = store;
	store_p = store_c + 1;
	//
	i = 0;
	*store_p++ = fload[i++];
	do {
		// scan for sequence
		repmax = 2;
		if (i > 255) offset = i - 256; else offset = 0;
		do {
			repsize = 0;
			while (fload[offset + repsize] == fload[i + repsize] && i + repsize < filesize && repsize < 129) {
				repsize++;
			}
			if (repsize > repmax) {
				repmax = repsize;
				offmax = i - offset;
			}
			offset++;
		} while (offset < i && repmax < 129);
		if (repmax > 2) {
			if (litsize > 0) {
				*store_c = litsize - 1;
				store_c = store_p++;
				litsize = 0;
			}
			*store_p++ = offmax - 1; //1-256 -> 0-255
			*store_c = repmax + 126;
			store_c = store_p++;
			i += repmax;
		}
		else {
			litsize++;
			*store_p++ = fload[i++];
			if (litsize > 127) {
				*store_c = litsize - 1;
				store_c = store_p++;
				litsize = 0;
			}
		}
	} while (i < filesize);
	if (litsize > 0) {
		*store_c = litsize - 1;
		store_c = store_p++;
	}
	*store_c = 128;	// end marker
	return store_p - store;
}