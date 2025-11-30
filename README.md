# ZX PicoIF2Lite Carousel Edition

Based on v0.6 of ZX PicoIF2Lite (see main branch)


## The Carousel 



The Interface is very simple with the Pico doing most of the heavy lifting. I've used 74LVC245 bus transceivers (as per Derek's original) to level shift both the address line (A0-A13) and data line (D0-D7) between the Pico and the edge connector. These chips also have the advantage of a high impedance mode when they are not required, something used on data line when the ROM isn't being accessed. The other chip in the circuit is a 4075 tri-input OR chip which is used to combine the A14, A15 & MREQ signals, which is basically indicating a ROM access. As the 4075 has three logic units I've also used this for the joystick.

Power for the Pico is provided by the 5V of the Spectrum via the edge connector, protected by a diode. I've also wired up a run button to easily reset the Pico and a user button which is used to control the Pico behaviour. More details of how to use the interface are below.

## Building Banners

![image](./images/createbanner.png "Create Banner")


## Version Control
- v0.7 Release version based on v0.6 of ZX PicoIF2Lite

## Usage
Usage is very simple. On every cold boot the Interface will be off meaning the Spectrum will boot as if nothing attached. To activate the interface press and hold the user button for >1second, the Spectrum will now boot into the ROM Explorer. If you just want to reset the Spectrum just press the user button and do not hold down. The ROM Explorer is very easy to use and is in the style of a standard File Explorer. Use the cursor/arrow keys (5-left, 6-down, 7-up, 8-right and no need to press shift) to navigate the ROMs and enter to select one. ROMs with icons to the right hand side indicate they will launch with [ZXC2 cartridge (ZXC)](#zxc2-cartridge-compatibility) or [Z80/SNA (Z80) compatibility](#z80--sna-snapshot-compatibility).

To indicate that ZX PicoIF2Lite is controlling the ROMCS line the LED on the Pico will be on. If it relinquishes control of ROMCS the LED will go out. You can see this with some ZXC compatible ROMS, converted snapshots and if you turn the unit off.

More details of the [ROM Explorer](#the-rom-selector) can be found below.





![image](./images/back.jpg "Case Back")
