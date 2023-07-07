// Versions
// v0.1 initial release taken form PicoIF2ROM but with interrupt drive user button
// v0.2 changes to improve ZXC compatibility, added zxcOn true/false
// v0.3 Z80/SNA snapshot support, zxcOn->compatMode
// v0.4 fixed issue with not loading correctly on earlier Spectrums, needed i register setting at 0x80
// v0.5 big ZX Spectrum machine code refactoring, LED matches ROMCS on/off, attempt to fix crash on reset
// v0.6 simplified ROM includes, added header to each ROM to replace romName & compatMode 
//
#define PROG_NAME   "ZX PicoIF2Lite"
#define VERSION_NUM "v0.6"
// ---------------------------------------------------------------------------
// includes
// ---------------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "picoif2lite.pio.h"
//#include "picoif2lite.h"   // header
//#include "picoif2lite_jh.h"   // header
#include "picoif2lite_lite.h"   // header (lite version for GitHub)
// ---------------------------------------------------------------------------
// gpio pins
// ---------------------------------------------------------------------------
#define PIN_A0      0   // GPIO 0-13 for A0-A13
#define PIN_D0      14  // GPIO 15-21 for D0-D7
#define PIN_LED     25  // Default LED pin for Pico (not W)
//                  3         2         1   
//                 10987654321098765432109876543210
#define MASK_LED 0b00000010000000000000000000000000

//
#define PIN_RESET   28  // GPIO to control RESET of Spectrum 
#define PIN_USER    22  // User input GPIO (v1.1 PCB this is 22)
#define PIN_ROMRQ   26  // ROM Request
#define PIN_ROMCS   27  // ROMCS
//
#define poMask   0b0011111111010000
#define poMaskn  0b0011111111000000
#define lkMask   0b0011111111100000
#define bkMask   0b0000000000001111
//
const uint8_t MAXROMS=*(&roms + 1) - roms; // test
uint8_t bank1[131072];   // equivalent to a 128K EPROM
uint8_t romSelector[16384];
volatile uint8_t rompos=0;     
volatile bool pagingOn=false;    
volatile uint32_t adder=0;     
PIO pio;
uint addr_data_sm;
//
void dtoBuffer(uint8_t *to,const uint8_t *from);
uint16_t simplelz(uint8_t* fload,uint8_t* store,uint16_t filesize);
void resetButton(uint gpio,uint32_t events);
//
void main() {
    // ---------------------------------------------------------------------
    // build the ROM Selector ROM from picoif2_lite.h, only need to do this once
    //   ** this is specific to the ROM Explorer ROM **
    // ---------------------------------------------------------------------
    dtoBuffer(romSelector,roms[0]); // ROM Explorer ROM into romSelector
    // 0x0123 maxroms (-1)
    // 0x0160 maxpages
    romSelector[0x012e]=MAXROMS-1;
    romSelector[0x016b]=((MAXROMS-1)/21)+1;                
    uint16_t bpos=0;
    uint8_t romnum=0;          
    // build text in bank 1
    do {
        if(romnum==MAXROMS-1) bank1[bpos++]=31;
        else bank1[bpos++]=30;
        uint i=0;
        do {
            if(roms[romnum][2+i]<0x20||roms[romnum][2+i]>=0x80) {
                bank1[bpos++]=0x20;
            } else {
                bank1[bpos++]=roms[romnum][2+i];
            }                
            i++;
        } while(roms[romnum][2+i]!=0&&i<32);
        if(roms[romnum][0]==1) {
            bank1[bpos++]=9;
            bank1[bpos++]=28;
            bank1[bpos++]=29;
        } else if(roms[romnum][0]==3||roms[romnum][0]==8) { // 48k or 128k snapshot
            bank1[bpos++]=9;
            bank1[bpos++]=26;
            bank1[bpos++]=27;
        }
        if(romnum==MAXROMS-1||(romnum+1)%21==0) {
            bank1[bpos++]=0;
        }
        else {
            bank1[bpos++]=10;
        }
    } while(++romnum!=MAXROMS);
    uint16_t compsize=simplelz(bank1,&romSelector[0x1e00],bpos);  // compress the text and put into romSelector               
    // -------------------------------
    // set-up user, romcs & reset gpio
    // -------------------------------
    gpio_init(PIN_RESET);
    gpio_set_dir(PIN_RESET,GPIO_OUT);
    gpio_put(PIN_RESET,false);    // hold in RESET state till ready
    //
    gpio_init(PIN_USER);
    gpio_set_dir(PIN_USER,GPIO_IN);
    gpio_pull_up(PIN_USER); // button active when connected to ground
    gpio_set_irq_enabled_with_callback(PIN_USER,GPIO_IRQ_EDGE_FALL,true,&resetButton);  // when user button pressed, interrupt code annd run resetButton routine
    //
    gpio_init(PIN_ROMCS);
    gpio_set_dir(PIN_ROMCS,GPIO_OUT);
    gpio_put(PIN_ROMCS,false);    // start with ROM off  
    //
    gpio_init(PIN_LED);
    gpio_set_dir(PIN_LED,GPIO_OUT);
    gpio_put(PIN_LED,false);
    // ----------
    // Set-up PIO
    // ----------
    pio=pio0; // use pio 0
    addr_data_sm=pio_claim_unused_sm(pio,true); // grab an free state machine from PIO 0
    uint addr_data_offset=pio_add_program(pio,&picoif2_program); // get instruction memory offset for the loaded program
    pio_sm_config addr_data_config=picoif2_program_get_default_config(addr_data_offset); // get the default state machine config
    // set-up IN pins
    for(uint i=PIN_A0;i<PIN_A0+14;i++) {
        pio_gpio_init(pio,i); // initialise all 14 input pins 
    }    
    pio_sm_set_consecutive_pindirs(pio,addr_data_sm,PIN_A0,14,false); // input
    sm_config_set_in_pins(&addr_data_config,PIN_A0); // set IN pin base
    sm_config_set_in_shift(&addr_data_config,false,true,14); // shift left 14 pins (A13-A0) into ISR, autopush resultant 32bit address to DMA RXF
    pio_gpio_init(pio,PIN_ROMRQ);
    pio_sm_set_consecutive_pindirs(pio,addr_data_sm,PIN_ROMRQ,1,false); // input
    // set-up OUT data pins
    for(uint i=PIN_D0;i<PIN_D0+8;i++) {
        pio_gpio_init(pio,i); // initialise all 8 output pins
    }
    sm_config_set_out_pins(&addr_data_config,PIN_D0,8); // set OUT pin base & number (bits)
    sm_config_set_out_shift(&addr_data_config,true,true,8); // right shift 8 bits of OSR to pins (D0-D7) with autopull on
    pio_sm_set_consecutive_pindirs(pio,addr_data_sm,PIN_D0,8,true); // set all output pins to output, D0-D7
    pio_sm_init(pio,addr_data_sm,addr_data_offset,&addr_data_config); // reset state machine and configure it
    // start PIO state machine
    pio_sm_set_enabled(pio,addr_data_sm,true); // enable state machine   
    busy_wait_us_32(50000);       // wait 50ms before lifting RESET          
    gpio_put(PIN_RESET,true);    // release RESET    
    // loop forever
    uint32_t address;
    uint32_t c;
    while(true) {
        address=pio_sm_get_blocking(pio,addr_data_sm);
        pio_sm_put_blocking(pio,addr_data_sm,bank1[address+adder]); // if ROMCS off then direction of Data chip is input so they do not interfere
        // z80 routine
        if((roms[rompos][0]==3||roms[rompos][0]==8)) {      
            if(address==0x3fff) {
                if(pagingOn==true) {
                    gpio_xor_mask(MASK_LED);
                    adder+=16384;
                    if(adder==roms[rompos][0]*16384) {
                        adder=0;
                        pagingOn=false;
                    }
                } else {
                    gpio_put(PIN_ROMCS,false);    // ROM off
                    gpio_put(PIN_LED,false);                    
                }
            }
        } else if(roms[rompos][0]==1&&pagingOn==true) {
                // top 64 ROM locations (0x3fc0-0x3fff)
                // 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
                //  0  0  1  1  1  1  1  1  1  1  l  p  b  b  b  b
                // (l)ock - set to 1 to prevent further paging (0x3fe0)
                // (p)age out - set to 1 to page out the ROM cartridge, 0 to page back in (0x3fd0)
                // (b)ank - select between the 16 banks
                if(address>=0x3fc0) {
                    // page in/out
                    if((address&poMask)==poMask) {
                        gpio_put(PIN_ROMCS,false);     // turn off ROMCS  
                        gpio_put(PIN_LED,false);     
                    } else {
                        gpio_put(PIN_ROMCS,true);     // turn on ROMCS 
                        gpio_put(PIN_LED,true);       
                    }  
                    // bank 0-7 (not enough memory for all 16 banks, only 8 allowed)
                    adder=(address&bkMask)*16384;
                    if(adder>131072) adder=0;
                    // lock paging
                    if((address&lkMask)==lkMask) {
                        pagingOn=false;
                    }
            }
        }
    }
}        
//
// ---------------------------------------------------------------------------
// resetButton - interrupt driven routine when user button pressed
// input:
//   gpio - which gpio called this routine
//   events - the compressed storage
// ---------------------------------------------------------------------------
void resetButton(uint gpio,uint32_t events) {
    uint32_t address;         
    busy_wait_us_32(100000);    // litle wait to help with button bounce
    gpio_put(PIN_RESET,false); // put Spectrum in RESET state                      
    // wait for button release and check held for 1second to switch ROM otherwise just reset
    uint64_t lastPing=time_us_64();        
    do {
        busy_wait_us_32(100000); // wait 100ms between each read, minimum 200ms wait on each press
    } while((gpio_get(PIN_USER)==false)&&(time_us_64()<lastPing+1000000));
    // button pressed for >=1second?
    if(time_us_64()>=lastPing+1000000) {                                              
        romSelector[0x0009]=rompos;   // 0x0005 current rom ** this is specific to the ROM Explorer ROM **
        romSelector[0x000e]=rompos-((rompos/21)*21);  // 0x000a current pos ** this is specific to the ROM Explorer ROM **
        romSelector[0x0013]=(rompos/21)+1;    // 0x000f current page ** this is specific to the ROM Explorer ROM ** 
        // run the Selector ROM          
        gpio_put(PIN_ROMCS,true);     // turn on ROMCS  
        gpio_put(PIN_RESET,true);   // lift reset         
        // check for ROM crash but looking for 10 im1 interupts (0x0038) in 1/2 second, if these aren't received then reset the ROM and try again
        uint16_t countAddress=0;
        lastPing=time_us_64();
        do {
            address=pio_sm_get_blocking(pio,addr_data_sm);
            pio_sm_put_blocking(pio,addr_data_sm,romSelector[address]);             
            if(address=0x0038) countAddress++;            
            else if(time_us_64()>=lastPing+500000) {
                gpio_put(PIN_RESET,false);   // lift reset  
                busy_wait_us_32(100000);
                gpio_put(PIN_RESET,true);   // lift reset  
                lastPing=time_us_64();
                countAddress=0;
            }
        } while(countAddress<10);
        //
        gpio_put(PIN_LED,true);          
        countAddress=0;                    
        do {
            address=pio_sm_get_blocking(pio,addr_data_sm);
            pio_sm_put_blocking(pio,addr_data_sm,romSelector[address]); 
            if(address>=0x3f80) countAddress++;
        } while(countAddress<256);    // wait for consistent signal above 0x3f80 from ROM selector 
        // ROM selected
        gpio_put(PIN_RESET,false); // put Spectrum in RESET state
        rompos=address-0x3f80;

        if(rompos>=MAXROMS) {
            rompos=MAXROMS-1; // error trap
        }                        
        dtoBuffer(bank1,roms[rompos]);  // unpack correct ROM                                                                    
    }
    // final set-up before restart
    if(roms[rompos][0]==1||roms[rompos][0]==3||roms[rompos][0]==8) {
        pagingOn=true; // if the ROM had this off make sure it is back on
    }
    if(rompos==0) {
        gpio_put(PIN_ROMCS,false);     // turn off ROMCS  
        gpio_put(PIN_LED,false);     
    } else {
        gpio_put(PIN_ROMCS,true);     // turn on ROMCS 
        gpio_put(PIN_LED,true);       
    }    
    adder=0;
    busy_wait_us_32(100000);    // wait 100ms before lifting RESET       
    gpio_put(PIN_RESET,true);   // lift reset    
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
    uint i=0,j=34,k; // start j at 34 to skip header
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
