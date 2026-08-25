#pragma once

// Schmale Win32-Schicht.
//
// OBVR.dll wird in zwei Umgebungen gebaut:
//   * MSVC / clang-cl unter Windows mit vollem Windows SDK
//   * clang --target=i686-pc-windows-msvc unter Linux ohne SDK
//
// Im zweiten Fall existiert kein <windows.h>. Statt das Projekt an eine
// Cross-Toolchain mit SDK zu binden, deklariert dieser Header die wenigen
// benoetigten Importe selbst. Die DLL braucht nur kernel32 und msvcrt,
// beide sind im Oblivion-Prozess ohnehin geladen.

#include "core/Types.h"

#if defined(OBVR_NO_WINSDK)

using HANDLE = void*;
using HMODULE = void*;
using BOOL = int;
using DWORD = unsigned long;

#define OBVR_STDCALL __stdcall
#define OBVR_IMPORT extern "C" __declspec(dllimport)

// Kein constexpr: reinterpret_cast ist in konstanten Ausdruecken unzulaessig.
inline HANDLE InvalidHandle() { return reinterpret_cast<HANDLE>(-1); }

constexpr DWORD PAGE_EXECUTE_READWRITE = 0x40;
constexpr DWORD MEM_COMMIT = 0x1000;
constexpr DWORD MEM_RESERVE = 0x2000;
constexpr DWORD GENERIC_WRITE = 0x40000000;
constexpr DWORD FILE_SHARE_READ = 0x1;
constexpr DWORD CREATE_ALWAYS = 2;
constexpr DWORD FILE_ATTRIBUTE_NORMAL = 0x80;

OBVR_IMPORT BOOL OBVR_STDCALL VirtualProtect(void* address, UInt32 size, DWORD newProtect, DWORD* oldProtect);
OBVR_IMPORT void* OBVR_STDCALL VirtualAlloc(void* address, UInt32 size, DWORD allocationType, DWORD protect);
OBVR_IMPORT HANDLE OBVR_STDCALL GetCurrentProcess();
OBVR_IMPORT BOOL OBVR_STDCALL FlushInstructionCache(HANDLE process, const void* baseAddress, UInt32 size);
OBVR_IMPORT HANDLE OBVR_STDCALL CreateFileA(const char* fileName, DWORD access, DWORD shareMode, void* security, DWORD creation, DWORD flags, HANDLE templateFile);
OBVR_IMPORT BOOL OBVR_STDCALL WriteFile(HANDLE file, const void* buffer, DWORD bytes, DWORD* written, void* overlapped);
OBVR_IMPORT BOOL OBVR_STDCALL CloseHandle(HANDLE object);
OBVR_IMPORT void OBVR_STDCALL OutputDebugStringA(const char* text);
OBVR_IMPORT DWORD OBVR_STDCALL GetPrivateProfileStringA(const char* section, const char* key, const char* defaultValue, char* buffer, DWORD size, const char* fileName);
OBVR_IMPORT DWORD OBVR_STDCALL GetModuleFileNameA(HMODULE module, char* fileName, DWORD size);

// For the OpenVR backend. openvr_api.dll is loaded at runtime rather than
// linked, so that OBVR still loads without SteamVR installed.
//
// GetProcAddress returns void* here instead of FARPROC. The call site casts
// to the concrete function pointer anyway, and replicating FARPROC would be
// one more declaration for no gain.
OBVR_IMPORT HMODULE OBVR_STDCALL LoadLibraryA(const char* fileName);
OBVR_IMPORT void* OBVR_STDCALL GetProcAddress(HMODULE module, const char* name);
OBVR_IMPORT BOOL OBVR_STDCALL FreeLibrary(HMODULE module);

// Aus msvcrt.dll. Bewusst sin/cos statt sinf/cosf: die float-Varianten fehlen
// in aelteren msvcrt-Staenden, die double-Varianten sind ueberall vorhanden.
extern "C" int __cdecl _snprintf(char* buffer, unsigned int count, const char* format, ...);
extern "C" double __cdecl atof(const char* text);
extern "C" double __cdecl sin(double value);
extern "C" double __cdecl cos(double value);

#else

#include <windows.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

inline HANDLE InvalidHandle() { return INVALID_HANDLE_VALUE; }

#endif
