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

add_executable(picozxcx picozxcx.c)

pico_generate_pio_header(picozxcx ${CMAKE_CURRENT_LIST_DIR}/picozxcx.pio)

target_link_libraries(picozxcx pico_stdlib hardware_pio)

pico_enable_stdio_usb(picozxcx 1) 
pico_enable_stdio_uart(picozxcx 0) 

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(picozxcx)

