cmake_minimum_required(VERSION 3.13)
project(utils C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

add_executable(z80torom ../z80torom.c)
add_executable(compressrom ../compressROM.c)

set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/../../..)
install(
        TARGETS z80torom compressrom
        DESTINATION ${CMAKE_INSTALL_PREFIX}
)
