# Verifikationsbau unter Linux, ohne Windows SDK und ohne MSVC.
#
# Zweck: pruefen, dass OBVR sauber zu einer 32-Bit-Windows-DLL uebersetzt und
# linkt, ohne dafuer eine Windows-Maschine oder einen mehrere Gigabyte grossen
# SDK-Download zu brauchen. Moeglich ist das, weil OBVR ausser kernel32 und
# msvcrt nichts benoetigt und die Importe in cmake/imports selbst erzeugt
# werden.
#
# Benutzung:
#   cmake -B build --toolchain cmake/toolchain-linux-nosdk.cmake
#   cmake --build build
#
# Der regulaere Weg fuer ein Release bleibt MSVC unter Windows. Diese
# Toolchain baut ohne C++-Standardbibliothek und ohne Ausnahmen; sie ist
# bewusst eine Pruefumgebung, keine Komfortumgebung.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86)

find_program(OBVR_CLANG clang REQUIRED)
find_program(OBVR_LLD_LINK lld-link REQUIRED)
find_program(OBVR_DLLTOOL llvm-dlltool REQUIRED)

set(CMAKE_CXX_COMPILER "${OBVR_CLANG}")
set(CMAKE_CXX_COMPILER_TARGET i686-pc-windows-msvc)
set(CMAKE_LINKER "${OBVR_LLD_LINK}")

set(OBVR_NO_WINSDK ON CACHE BOOL "Ohne Windows SDK bauen" FORCE)

# Ohne CRT laesst sich kein vollstaendiges Testprogramm linken; CMake soll
# seinen Compilertest deshalb nur uebersetzen, nicht linken.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(OBVR_FREESTANDING_FLAGS
	"-ffreestanding -fno-exceptions -fno-rtti -nostdinc++ -fno-stack-protector")

set(CMAKE_CXX_FLAGS_INIT "${OBVR_FREESTANDING_FLAGS}")

# CMake haengt sonst die ueblichen Windows-Bibliotheken an (user32, gdi32,
# ole32 ...). Die gibt es ohne SDK nicht, und OBVR braucht sie auch nicht -
# die noetigen Importe kommen aus cmake/imports.
set(CMAKE_CXX_STANDARD_LIBRARIES "")

# Ohne CRT existiert kein _DllMainCRTStartup, der Einsprungpunkt muss deshalb
# explizit auf DllMain zeigen. Die @12-Dekoration ist die stdcall-Signatur.
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-Xlinker /nodefaultlib -Xlinker /entry:DllMain@12")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BEFORE)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
