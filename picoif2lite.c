// Versions
// v0.1 initial release taken form PicoIF2ROM but with interrupt drive user button
// v0.2 changes to improve ZXC compatibility, added zxcOn true/false
// v0.3 Z80/SNA snapshot support, zxcOn->compatMode
// v0.4 fixed issue with not loading correctly on earlier Spectrums, needed i register setting at 0x80
// v0.5 big ZX Spectrum machine code refactoring, LED matches ROMCS on/off, attempt to fix crash on reset
// v0.6 simplified ROM includes, added header to each ROM to replace romName & compatMode
// v0.7 boot into 1st ROM if hold down RESET on boot
// v0.7c special carousel edition based on 0.7
//
#define PROG_NAME   "ZX PicoIF2Lite"
#define VERSION_NUM "v0.7c"
// ---------------------------------------------------------------------------
// includes
// ---------------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "picoif2lite.pio.h"
#include "picoif2lite.h"   // header
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
const uint8_t MAXROMS=*(&roms + 1) - roms; // calculate MAXROMs from rom array 
//
void dtoBuffer(volatile uint8_t *to,const uint8_t *from);
//
void __no_inline_not_in_flash_func(main)() {            
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
    PIO pio=pio0; // use pio 0
    uint addr_data_sm=pio_claim_unused_sm(pio,true); // grab an free state machine from PIO 0
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
    // start PIO state machine
    pio_sm_init(pio,addr_data_sm,addr_data_offset,&addr_data_config); // reset state machine and configure it
    pio_sm_set_enabled(pio,addr_data_sm,true); // enable state machine   
    //
    volatile uint8_t bank1[131072];   // equivalent to a 128K EPROM
    volatile uint32_t adder=0;        
    volatile uint32_t address;    
    volatile bool ROMOn=false;  
    bool pagingOn=false;    
    uint8_t rompos=0;   
    busy_wait_us_32(100000);       // wait 100ms before lifting RESET          
    gpio_put(PIN_RESET,true);    // release RESET
    // loop forever
    while(true) {      
        // button press
        if(gpio_get(PIN_USER)==false) { 
            gpio_put(PIN_RESET,false); // put Spectrum in RESET state                   
            // wait for button release and check held for 1second to switch ROM otherwise just reset    
            uint16_t upCount=0;
            do {
                busy_wait_us_32(1000); // wait 1ms between each read so button has to be up for 100ms before moving on
                if(gpio_get(PIN_USER)==true) upCount++;
            } while (upCount<100);
            uint16_t countAddress=0;
            dtoBuffer(bank1,roms[rompos]); // Carousel ROM into romSelector                
            gpio_put(PIN_LED,true);                       
            gpio_put(PIN_ROMCS,true);   // turn on ROMCS              
            busy_wait_us_32(100000);    // wait 100ms before lifting RESET          
            gpio_put(PIN_RESET,true);   // lift reset         
            while(pagingOn==false) {
                address=pio_sm_get_blocking(pio,addr_data_sm);                     
                pio_sm_put_blocking(pio,addr_data_sm,bank1[address]); 
                if(address>=0x3f80) {
                    countAddress++;
                    if(countAddress==256) {
                        // ROM selected
                        gpio_put(PIN_RESET,false); // put Spectrum in RESET state
                        rompos=address-0x3f80;
                        if(rompos>=MAXROMS) {
                            rompos=MAXROMS-1; // error trap
                        }                        
                        dtoBuffer(bank1,roms[rompos]);  // unpack correct ROM    
                        pagingOn=true; // if the ROM had this off make sure it is back on
                        adder=0; // start at beginning of ROM
                        // relaunch               
                        busy_wait_us_32(100000);    // wait 100ms before lifting RESET       
                        gpio_put(PIN_RESET,true);   // lift reset                           
                    }
                } else if(gpio_get(PIN_USER)==false)  {  
                    gpio_put(PIN_RESET,false); // put Spectrum in RESET state                    
                    busy_wait_us_32(100000);    // wait 100ms before lifting RESET       
                    gpio_put(PIN_RESET,true);   // lift reset                     
                }
            }
            while(rompos!=0) {
                address=pio_sm_get_blocking(pio,addr_data_sm);
                pio_sm_put_blocking(pio,addr_data_sm,bank1[address+adder]); 
                // if countAddress>=256 then this kicks in to unpack the snapshot
                if(address==0x3fff) { // if 0x3fff accessed then switch the ROM 
                    if(pagingOn==true) {
                        gpio_xor_mask(MASK_LED); // toggle LED to show doing something
                        adder+=16384;
                        if(adder==roms[rompos][0]*16384) { // run out of ROMs?
                            adder=0;
                            pagingOn=false;
                        }
                    } 
                    else { // if paging is OFF then need to page in the original ROM and turn off ROM paging
                        rompos=0;
                        gpio_put(PIN_ROMCS,false);    // ROM offd
                        gpio_put(PIN_LED,false);              
                    }
                } else if(gpio_get(PIN_USER)==false)  {
                    adder=0;     
                    gpio_put(PIN_RESET,false); // put Spectrum in RESET state                    
                    busy_wait_us_32(100000);    // wait 100ms before lifting RESET       
                    gpio_put(PIN_RESET,true);   // lift reset                     
                }
            } 
        }
    }
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
void dtoBuffer(volatile uint8_t *to,const uint8_t *from) { 
    uint i=0,j=2,k; // start j at 2 to skip header
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