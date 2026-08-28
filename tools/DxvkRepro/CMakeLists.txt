cmake_minimum_required(VERSION 3.20)

# A standalone diagnostic, deliberately not part of the DLL build: it
# replays a recorded frame of Oblivion's dynamic-pool traffic against
# whatever d3d9.dll sits beside the executable - DXVK when its DLL is
# copied there, the system runtime otherwise - and reports whether two
# identical passes render identically. Built 32-bit like the game, so
# the DXVK x32 binaries are the ones under test.
#
#   cmake -B build tools/DxvkRepro
#   cmake --build build --config Release
#   copy <dxvk>\x32\d3d9.dll build\
#   build\dxvk_repro.exe docs\verification\pool-timeline-20260828.txt

project(DxvkRepro LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(dxvk_repro main.cpp)
target_link_libraries(dxvk_repro PRIVATE d3d9 user32)
