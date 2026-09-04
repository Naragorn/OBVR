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

#include <cstddef>

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
constexpr DWORD PAGE_READWRITE = 0x04;
constexpr DWORD MEM_COMMIT = 0x1000;
constexpr DWORD MEM_RESERVE = 0x2000;
// MEM_RELEASE requires a size of 0 and frees the whole reservation.
constexpr DWORD MEM_RELEASE = 0x8000;
constexpr DWORD GENERIC_WRITE = 0x40000000;
constexpr DWORD GENERIC_READ = 0x80000000;
constexpr DWORD FILE_SHARE_READ = 0x1;
constexpr DWORD CREATE_ALWAYS = 2;
constexpr DWORD OPEN_EXISTING = 3;
constexpr DWORD FILE_ATTRIBUTE_NORMAL = 0x80;
constexpr DWORD INVALID_FILE_ATTRIBUTES = 0xFFFFFFFF;
constexpr DWORD INVALID_FILE_SIZE = 0xFFFFFFFF;
constexpr DWORD MOVEFILE_REPLACE_EXISTING = 0x1;
constexpr DWORD MOVEFILE_WRITE_THROUGH = 0x8;

OBVR_IMPORT BOOL OBVR_STDCALL VirtualProtect(void* address, UInt32 size, DWORD newProtect, DWORD* oldProtect);
OBVR_IMPORT void* OBVR_STDCALL VirtualAlloc(void* address, UInt32 size, DWORD allocationType, DWORD protect);
OBVR_IMPORT BOOL OBVR_STDCALL VirtualFree(void* address, UInt32 size, DWORD freeType);
OBVR_IMPORT HANDLE OBVR_STDCALL GetCurrentProcess();
OBVR_IMPORT BOOL OBVR_STDCALL FlushInstructionCache(HANDLE process, const void* baseAddress, UInt32 size);
OBVR_IMPORT HANDLE OBVR_STDCALL CreateFileA(const char* fileName, DWORD access, DWORD shareMode, void* security, DWORD creation, DWORD flags, HANDLE templateFile);
OBVR_IMPORT int OBVR_STDCALL MoveFileExA(const char* existingName, const char* newName, DWORD flags);
OBVR_IMPORT BOOL OBVR_STDCALL DeleteFileA(const char* fileName);
OBVR_IMPORT DWORD OBVR_STDCALL GetFileSize(HANDLE file, DWORD* highSize);
OBVR_IMPORT BOOL OBVR_STDCALL ReadFile(HANDLE file, void* buffer, DWORD bytes, DWORD* read, void* overlapped);
OBVR_IMPORT BOOL OBVR_STDCALL WriteFile(HANDLE file, const void* buffer, DWORD bytes, DWORD* written, void* overlapped);
OBVR_IMPORT BOOL OBVR_STDCALL FlushFileBuffers(HANDLE file);
OBVR_IMPORT BOOL OBVR_STDCALL CloseHandle(HANDLE object);

// For the one thread OBVR runs: a poller that waits for Oblivion to build its
// Direct3D device, so the end of the frame can be hooked before the main menu
// is drawn rather than after the first world camera exists.
OBVR_IMPORT HANDLE OBVR_STDCALL CreateThread(void* attributes, UInt32 stackSize,
                                             DWORD(OBVR_STDCALL* start)(void*),
                                             void* parameter, DWORD flags, DWORD* threadId);
OBVR_IMPORT void OBVR_STDCALL Sleep(DWORD milliseconds);

// For the hang watchdog's report: it stops the render thread for a moment,
// reads where it stands and what return addresses lie on its stack, and
// lets it go again. The thread context is winnt.h's x86 CONTEXT, spelled
// out here field by field (SDK 10.0.26100 winnt.h: ContextFlags, six debug
// registers, the 112-byte 80387 save area, four segment registers, six
// integer registers, Ebp, Eip, SegCs, EFlags, Esp, SegSs, then 512 bytes of
// extended registers - 716 bytes), because the SDK-free branch has no
// windows.h. CONTEXT_CONTROL and CONTEXT_INTEGER carry the i386 flag.
constexpr DWORD CONTEXT_CONTROL = 0x00010001;
constexpr DWORD CONTEXT_INTEGER = 0x00010002;
constexpr DWORD THREAD_SUSPEND_RESUME = 0x0002;
constexpr DWORD THREAD_GET_CONTEXT = 0x0008;
constexpr DWORD THREAD_QUERY_INFORMATION = 0x0040;
constexpr DWORD PAGE_NOACCESS = 0x01;
constexpr DWORD PAGE_GUARD = 0x100;
struct ThreadContext;
struct MemoryBasicInfo;
OBVR_IMPORT DWORD OBVR_STDCALL GetCurrentThreadId();
OBVR_IMPORT HANDLE OBVR_STDCALL OpenThread(DWORD access, BOOL inherit, DWORD threadId);
OBVR_IMPORT DWORD OBVR_STDCALL SuspendThread(HANDLE thread);
OBVR_IMPORT DWORD OBVR_STDCALL ResumeThread(HANDLE thread);
OBVR_IMPORT BOOL OBVR_STDCALL GetThreadContext(HANDLE thread, ThreadContext* context);
OBVR_IMPORT UInt32 OBVR_STDCALL VirtualQuery(const void* address, MemoryBasicInfo* info,
                                             UInt32 length);
OBVR_IMPORT void OBVR_STDCALL OutputDebugStringA(const char* text);
OBVR_IMPORT DWORD OBVR_STDCALL GetPrivateProfileStringA(const char* section, const char* key, const char* defaultValue, char* buffer, DWORD size, const char* fileName);

// For writing Oblivion's own render resolution, which a headset wants square
// and a monitor wants wide. The game reads its INI once, at startup, so a
// change made here takes effect on the next run - which is stated in the log
// rather than hoped to be noticed.
OBVR_IMPORT BOOL OBVR_STDCALL WritePrivateProfileStringA(const char* section, const char* key, const char* value, const char* fileName);
OBVR_IMPORT DWORD OBVR_STDCALL GetEnvironmentVariableA(const char* name, char* buffer, DWORD size);
OBVR_IMPORT DWORD OBVR_STDCALL GetModuleFileNameA(HMODULE module, char* fileName, DWORD size);
OBVR_IMPORT DWORD OBVR_STDCALL GetFileAttributesA(const char* fileName);

// For the frame time the position smoothing needs. A high resolution counter
// rather than GetTickCount, whose resolution of 10 to 16 ms is of the same
// order as a frame itself.
//
// The counter is a 64-bit value, declared here as two 32-bit halves so that
// this header needs no 64-bit integer type of its own. x86 is little endian,
// so the low half comes first.
struct LargeInteger {
	UInt32 low;
	SInt32 high;
};

OBVR_IMPORT BOOL OBVR_STDCALL QueryPerformanceCounter(LargeInteger* count);
OBVR_IMPORT BOOL OBVR_STDCALL QueryPerformanceFrequency(LargeInteger* frequency);

// For the OpenVR backend. openvr_api.dll is loaded at runtime rather than
// linked, so that OBVR still loads without SteamVR installed.
//
// GetProcAddress returns void* here instead of FARPROC. The call site casts
// to the concrete function pointer anyway, and replicating FARPROC would be
// one more declaration for no gain.
OBVR_IMPORT HMODULE OBVR_STDCALL LoadLibraryA(const char* fileName);

// For checking whether Oblivion has already loaded d3d9.dll, which decides
// which of the two ways into its resolution is still open.
OBVR_IMPORT HMODULE OBVR_STDCALL GetModuleHandleA(const char* moduleName);

// For naming whoever else wrote into the Direct3D factory's method table.
// The module is asked for by address, because a pointer found in a shared
// slot says nothing about itself until it is placed in a file on disk.
constexpr DWORD GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT = 0x00000002;
constexpr DWORD GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS = 0x00000004;
OBVR_IMPORT BOOL OBVR_STDCALL GetModuleHandleExA(DWORD flags, const char* name, HMODULE* module);
OBVR_IMPORT void* OBVR_STDCALL GetProcAddress(HMODULE module, const char* name);
OBVR_IMPORT BOOL OBVR_STDCALL FreeLibrary(HMODULE module);

// From user32.dll, for the recenter key. This is the only import outside
// kernel32 and msvcrt, and it is a deliberate trade: the alternative would be
// to read Oblivion's own input state, which would mean one more reverse
// engineered address to keep correct. user32 is loaded in every GUI process,
// so it costs nothing at runtime.
OBVR_IMPORT short OBVR_STDCALL GetAsyncKeyState(int virtualKey);

// For the hand-tracked mode: the controllers drive the game through the
// keys and mouse buttons the player has bound, injected the way the input
// harness injects them - keybd_event with a scan code, which DirectInput
// on modern Windows is fed from (measured: the harness's cursor probe
// follows injected movement). The constants are winuser.h's.
constexpr DWORD KEYEVENTF_KEYUP = 0x0002;
constexpr DWORD KEYEVENTF_SCANCODE = 0x0008;
constexpr DWORD MOUSEEVENTF_MOVE = 0x0001;
constexpr DWORD MOUSEEVENTF_LEFTDOWN = 0x0002;
constexpr DWORD MOUSEEVENTF_LEFTUP = 0x0004;
constexpr DWORD MOUSEEVENTF_RIGHTDOWN = 0x0008;
constexpr DWORD MOUSEEVENTF_RIGHTUP = 0x0010;
constexpr DWORD MOUSEEVENTF_WHEEL = 0x0800;
constexpr int WHEEL_DELTA = 120;
constexpr UInt32 MAPVK_VK_TO_VSC = 0;
OBVR_IMPORT void OBVR_STDCALL keybd_event(UInt8 virtualKey, UInt8 scanCode, DWORD flags,
                                          UInt32 extraInfo);
OBVR_IMPORT void OBVR_STDCALL mouse_event(DWORD flags, DWORD dx, DWORD dy, DWORD data,
                                          UInt32 extraInfo);
OBVR_IMPORT UInt32 OBVR_STDCALL MapVirtualKeyA(UInt32 code, UInt32 mapType);

// For sizing Oblivion's window when the fullscreen flag is cleared.
//
// This is not an extra: in exclusive fullscreen Direct3D sizes the game's
// window itself, so Oblivion never had to. Windowed, that stops happening and
// the window keeps whatever it was created with - 320x240, as DXVK reported
// when it built the swapchain behind a 3200x3200 back buffer. The mouse was
// mapped against that, and the device was eventually lost.
//
// RECT is spelled out here because the SDK-free branch has no windows.h. Its
// four LONGs are the same four fields in the same order either way.
struct WindowRect {
	SInt32 left;
	SInt32 top;
	SInt32 right;
	SInt32 bottom;
};

OBVR_IMPORT BOOL OBVR_STDCALL GetWindowRect(void* window, WindowRect* rect);
OBVR_IMPORT BOOL OBVR_STDCALL GetClientRect(void* window, WindowRect* rect);
OBVR_IMPORT BOOL OBVR_STDCALL SetWindowPos(void* window, void* insertAfter, int x, int y,
                                           int width, int height, UInt32 flags);

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

// One reading of the high resolution counter, and its frequency, regardless
// of which branch above supplied the import.
//
// The Windows SDK spells the argument LARGE_INTEGER while the SDK-free branch
// spells it LargeInteger. Both are two 32-bit halves in the same order, so the
// generated code is identical and only the name differs - which is exactly
// what these two wrappers hide, so that callers need no #if of their own.
#if defined(OBVR_NO_WINSDK)

inline long long ReadPerformanceCounter() {
	LargeInteger value{0, 0};
	QueryPerformanceCounter(&value);
	return (static_cast<long long>(value.high) << 32) | value.low;
}

inline long long ReadPerformanceFrequency() {
	LargeInteger value{0, 0};
	QueryPerformanceFrequency(&value);
	return (static_cast<long long>(value.high) << 32) | value.low;
}

#else

inline long long ReadPerformanceCounter() {
	LARGE_INTEGER value{};
	QueryPerformanceCounter(&value);
	return value.QuadPart;
}

inline long long ReadPerformanceFrequency() {
	LARGE_INTEGER value{};
	QueryPerformanceFrequency(&value);
	return value.QuadPart;
}

#endif

// The render thread's registers and one region of its stack, for the hang
// watchdog's report. Spelled out once for both branches: the x86 CONTEXT of
// winnt.h (SDK 10.0.26100) is ContextFlags, six debug registers, the
// 112-byte 80387 save area, four segment registers, six integer registers,
// Ebp, Eip, SegCs, EFlags, Esp, SegSs and 512 bytes of extended registers -
// 716 bytes - and MEMORY_BASIC_INFORMATION is seven 32-bit fields. The SDK
// branch checks both against the real types below.
struct ThreadContext {
	DWORD contextFlags;
	DWORD dr0, dr1, dr2, dr3, dr6, dr7;
	DWORD floatSave[28];
	DWORD segGs, segFs, segEs, segDs;
	DWORD edi, esi, ebx, edx, ecx, eax;
	DWORD ebp;
	DWORD eip;
	DWORD segCs;
	DWORD eflags;
	DWORD esp;
	DWORD segSs;
	UInt8 extendedRegisters[512];
};
static_assert(sizeof(ThreadContext) == 716, "x86 CONTEXT is 716 bytes");

struct MemoryBasicInfo {
	void* baseAddress;
	void* allocationBase;
	DWORD allocationProtect;
	UInt32 regionSize;
	DWORD state;
	DWORD protect;
	DWORD type;
};
static_assert(sizeof(MemoryBasicInfo) == 28, "x86 MEMORY_BASIC_INFORMATION is 28 bytes");

#if defined(OBVR_NO_WINSDK)

inline bool ReadThreadContext(HANDLE thread, ThreadContext& context) {
	context.contextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
	return GetThreadContext(thread, &context) != 0;
}

inline bool QueryMemory(const void* address, MemoryBasicInfo& info) {
	return VirtualQuery(address, &info, sizeof(info)) == sizeof(info);
}

#else

static_assert(sizeof(ThreadContext) == sizeof(CONTEXT), "ThreadContext mirrors CONTEXT");
static_assert(offsetof(ThreadContext, eip) == offsetof(CONTEXT, Eip), "Eip offset");
static_assert(offsetof(ThreadContext, esp) == offsetof(CONTEXT, Esp), "Esp offset");
static_assert(offsetof(ThreadContext, ebp) == offsetof(CONTEXT, Ebp), "Ebp offset");
static_assert(sizeof(MemoryBasicInfo) == sizeof(MEMORY_BASIC_INFORMATION),
              "MemoryBasicInfo mirrors MEMORY_BASIC_INFORMATION");
static_assert(offsetof(MemoryBasicInfo, protect) == offsetof(MEMORY_BASIC_INFORMATION, Protect),
              "Protect offset");

inline bool ReadThreadContext(HANDLE thread, ThreadContext& context) {
	CONTEXT* real = reinterpret_cast<CONTEXT*>(&context);
	real->ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
	return GetThreadContext(thread, real) != 0;
}

inline bool QueryMemory(const void* address, MemoryBasicInfo& info) {
	return VirtualQuery(address, reinterpret_cast<MEMORY_BASIC_INFORMATION*>(&info),
	                    sizeof(info)) == sizeof(info);
}

#endif
