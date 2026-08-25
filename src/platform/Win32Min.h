#pragma once

// A narrow Win32 layer.
//
// OBVR.dll is built in two environments:
//   * MSVC / clang-cl on Windows with the full Windows SDK
//   * clang --target=i686-pc-windows-msvc on Linux without the SDK
//
// In the second case there is no <windows.h>. Rather than tie the project to
// a cross toolchain that carries the SDK, this header declares the handful of
// needed imports itself. The DLL only needs kernel32, msvcrt and one function
// out of user32, and all three are loaded in the Oblivion process anyway.

#include "core/Types.h"

#if defined(OBVR_NO_WINSDK)

using HANDLE = void*;
using HMODULE = void*;
using BOOL = int;
using DWORD = unsigned long;

#define OBVR_STDCALL __stdcall
#define OBVR_IMPORT extern "C" __declspec(dllimport)

// Not constexpr: reinterpret_cast is not allowed in constant expressions.
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

// From user32.dll, for the recenter key. This is the only import outside
// kernel32 and msvcrt, and it is a deliberate trade: the alternative would be
// to read Oblivion's own input state, which would mean one more reverse
// engineered address to keep correct. user32 is loaded in every GUI process,
// so it costs nothing at runtime.
OBVR_IMPORT short OBVR_STDCALL GetAsyncKeyState(int virtualKey);

// From msvcrt.dll. Deliberately sin/cos rather than sinf/cosf: the float
// variants are missing from older msvcrt revisions, the double ones are
// present everywhere.
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
