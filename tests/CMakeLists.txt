cmake_minimum_required(VERSION 3.20)

# Eigenstaendiges Projekt, bewusst nicht Teil des DLL-Baus.
#
# OBVR.dll wird fuer 32-Bit-Windows uebersetzt, die Tests laufen dagegen nativ
# auf dem Entwicklungsrechner. Beides in einem CMake-Lauf hiesse, zwei
# Toolchains gleichzeitig zu fahren. Die geprueften Bausteine sind reine
# Byteerzeugung und haengen nicht von der Zielarchitektur ab.
#
#   cmake -B build-tests tests
#   cmake --build build-tests
#   ctest --test-dir build-tests --output-on-failure

project(OBVRTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

add_executable(trampoline_test
	TrampolineTest.cpp
	../src/camera/CameraTrampoline.cpp
	../src/core/CodeWriter.cpp
)

target_include_directories(trampoline_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(trampoline_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME trampoline COMMAND trampoline_test)

add_executable(quaternion_test
	QuaternionTest.cpp
	../src/vr/Quaternion.cpp
	../src/core/Rotation.cpp
)

target_include_directories(quaternion_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(quaternion_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME quaternion COMMAND quaternion_test)
