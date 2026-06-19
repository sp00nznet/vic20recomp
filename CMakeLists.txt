# vic20recomp - static-recompilation toolkit for Commodore VIC-20 cartridges.
#
# Builds two things:
#   * vic20recomp   - the runtime library recompiled game code links against
#                     (6502 CPU state, the flat VIC-20 memory map, the VIC video
#                      readout). No SDL dependency at this layer.
#   * m6502recomp   - the recompiler CLI (decode / analyze / emit).
#
# A game repo consumes this by pointing VIC20RECOMP_DIR at it and calling
# add_subdirectory(). See README.md.
cmake_minimum_required(VERSION 3.16)
project(vic20recomp C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# --- runtime library -------------------------------------------------------
add_library(vic20recomp STATIC
    src/cpu.c
    src/mem.c
    src/recomp_rt.c
    src/video.c
)
target_include_directories(vic20recomp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# --- recompiler CLI --------------------------------------------------------
add_subdirectory(tools/m6502recomp)
