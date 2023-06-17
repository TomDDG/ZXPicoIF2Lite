cmake_minimum_required(VERSION 3.13)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(oled C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
#set(PICO_BOARD pico_w)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# rest of your project

add_executable(picoif2lite picoif2lite.c)

pico_generate_pio_header(picoif2lite ${CMAKE_CURRENT_LIST_DIR}/picoif2lite.pio)

target_link_libraries(picoif2lite pico_stdlib hardware_pio)

pico_enable_stdio_usb(picoif2lite 1) 
pico_enable_stdio_uart(picoif2lite 0) 

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(picoif2lite)

