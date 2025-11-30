# ZX PicoIF2Lite Carousel Edition

Based on v0.6 of ZX PicoIF2Lite ([see main branch](https://github.com/TomDDG/ZXPicoIF2Lite/tree/main))


## The Carousel 

![image](./images/carousel1.png "Default Carousel")

Once selected the Carousel will load the snapshot using the ROM loader.

![image](./images/carousel2.png "Loading")

## Building Banners

The ````buildheader```` software will create banners for each snapshot from the initial screen, usually the loading screen. This is taken from the top 8 attribute rows (64 pixels). Sometimes this will be ok, sometimes you will be better creating a custom banner. To do this simply create a new screen with the top 8 rows being the banner. Example shown:

![image](./images/createbanner.png "Create Banner")

## Version Control
- v0.7 Release version based on v0.6 of ZX PicoIF2Lite

## Usage
Usage is very simple. On every cold boot the Interface will be off meaning the Spectrum will boot as if nothing attached. To activate the interface press the user button, the Spectrum will now boot into the Carousel. The Carousel is very easy to use with the keys 5 for left, 8 for right and enter to load. The attached joystick left, right and fire also works.
