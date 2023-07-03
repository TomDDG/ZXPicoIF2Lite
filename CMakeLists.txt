cmake_minimum_required(VERSION 3.13)

#set(PICO_SDK_FETCH_FROM_GIT on)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(picoif2lite C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
#set(PICO_BOARD pico_w)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

ExternalProject_Add(
        utils
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/utils
        BUILD_COMMAND make
        INSTALL_DIR ${CMAKE_BINARY_DIR}
)

function(compress_rom ROM_DIR ROM_NAME ROM_TITLE)

    # Compress the ROM image
    add_custom_command(
            TARGET roms
            PRE_BUILD
            WORKING_DIRECTORY "${ROM_DIR}"
            COMMAND ${CMAKE_BINARY_DIR}/compressrom -t "${ROM_TITLE}" "${ROM_NAME}"
            DEPENDS ${PROJECT_NAME}
            COMMENT "  COMPRESS_ROM   ${ROM_NAME} \"${ROM_TITLE}\""
            VERBATIM
    )

endfunction()

add_custom_target(
        roms
        COMMAND echo "Compressing ROMs..."
)

compress_rom("${CMAKE_SOURCE_DIR}/roms" "ROM-Explorer.rom" "Turn ZX PicoIF2Lite Off")

function(compress_z80 Z80_DIR Z80_NAME)

    # Compress the ROM image
    add_custom_command(
            TARGET snapshots
            PRE_BUILD
            WORKING_DIRECTORY "${Z80_DIR}"
            COMMAND ${CMAKE_BINARY_DIR}/z80torom "${Z80_NAME}"
            DEPENDS ${PROJECT_NAME}
            COMMENT "  COMPRESS_Z80   ${Z80_NAME}"
            VERBATIM
    )

endfunction()

add_custom_target(
        snapshots
        COMMAND echo "Compressing Z80 snapshots..."
)

file(
        GLOB_RECURSE Z80_SNAPSHOTS
        LIST_DIRECTORIES false
        RELATIVE "${CMAKE_SOURCE_DIR}/snapshots/"
        "${CMAKE_SOURCE_DIR}/snapshots/*.z80"
)
foreach (snapshot ${Z80_SNAPSHOTS})
    compress_z80("${CMAKE_SOURCE_DIR}/snapshots" ${snapshot})
endforeach (snapshot)

file(GLOB ROM_SRC "${CMAKE_SOURCE_DIR}/roms/*.h")
file(GLOB Z80_SRC "${CMAKE_SOURCE_DIR}/snapshots/*.c")

add_executable(${PROJECT_NAME} picoif2lite.c rom_descriptor.c ${ROM_SRC} ${Z80_SRC})

add_dependencies(${PROJECT_NAME} roms snapshots)

pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/picoif2lite.pio)

target_link_libraries(${PROJECT_NAME} pico_stdlib hardware_pio)

pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(${PROJECT_NAME})

