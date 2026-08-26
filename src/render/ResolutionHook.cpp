#include "render/ResolutionHook.h"

#include "core/Log.h"
#include "game/GameAddresses.h"
#include "platform/ImportHook.h"
#include "platform/Win32Min.h"
#include "render/D3D9Types.h"

namespace obvr::render {
namespace {

UInt32 g_wantedWidth = 0;
UInt32 g_wantedHeight = 0;

d3d9::Direct3DCreate9Fn g_originalCreate9 = nullptr;
d3d9::CreateDeviceFn g_originalCreateDevice = nullptr;

using GetProcAddressFn = void*(__stdcall*)(void* module, const char* name);
GetProcAddressFn g_originalGetProcAddress = nullptr;

bool g_deviceCreated = false;
UInt32 g_createdWidth = 0;
UInt32 g_createdHeight = 0;

bool Equals(const char* a, const char* b) {
	if (a == nullptr || b == nullptr) {
		return false;
	}
	while (*a != '\0' && *a == *b) {
		++a;
		++b;
	}
	return *a == *b;
}

// Where the frame's size is actually decided.
//
// Everything else in this file exists to get here before the game does. The
// parameters are the game's own, changed in place: the runtime reads them
// after this returns, so a change made here is a change to what is built.
SInt32 __stdcall HookedCreateDevice(void* self, UInt32 adapter, UInt32 deviceType,
                                    void* focusWindow, UInt32 behaviourFlags,
                                    d3d9::PresentParameters* parameters, void** device) {
	if (parameters != nullptr) {
		const UInt32 wasWidth = parameters->backBufferWidth;
		const UInt32 wasHeight = parameters->backBufferHeight;

		if (g_wantedWidth != 0) {
			parameters->backBufferWidth = g_wantedWidth;
		}
		if (g_wantedHeight != 0) {
			parameters->backBufferHeight = g_wantedHeight;
		}

		g_deviceCreated = true;
		g_createdWidth = parameters->backBufferWidth;
		g_createdHeight = parameters->backBufferHeight;

		OBVR_LOG("Resolution: the game asked for %ux%u, and is getting %ux%u", wasWidth,
		         wasHeight, g_createdWidth, g_createdHeight);
	}

	return g_originalCreateDevice(self, adapter, deviceType, focusWindow, behaviourFlags,
	                              parameters, device);
}

// Catches the factory on its way out of Direct3DCreate9, for the sole purpose
// of patching the one method on it that matters.
//
// Nothing else about the factory is touched, and the pointer is handed back
// exactly as it arrived.
void* __stdcall HookedCreate9(UInt32 sdkVersion) {
	void* factory = g_originalCreate9 != nullptr ? g_originalCreate9(sdkVersion) : nullptr;
	if (factory == nullptr || g_originalCreateDevice != nullptr) {
		return factory;
	}

	auto** vtable = *reinterpret_cast<void***>(factory);
	if (vtable == nullptr || vtable[d3d9::kFactoryCreateDevice] == nullptr) {
		OBVR_LOG("Resolution: the Direct3D factory has no usable method table, so the "
		         "resolution is the game's own");
		return factory;
	}

	void** slot = &vtable[d3d9::kFactoryCreateDevice];

	DWORD protection = 0;
	if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &protection)) {
		OBVR_LOG("Resolution: the factory's method table could not be made writable");
		return factory;
	}

	g_originalCreateDevice = reinterpret_cast<d3d9::CreateDeviceFn>(*slot);
	*slot = reinterpret_cast<void*>(&HookedCreateDevice);

	DWORD ignored = 0;
	VirtualProtect(slot, sizeof(void*), protection, &ignored);

	OBVR_LOG("Resolution: CreateDevice hooked at factory entry %u",
	         d3d9::kFactoryCreateDevice);
	return factory;
}

// The way in, because Oblivion has no d3d9 import to replace.
//
// It loads the library by hand and looks the function up by name, so the name
// is where OBVR gets in the way. Every other lookup in the process passes
// through unchanged and unexamined beyond one character.
void* __stdcall HookedGetProcAddress(void* module, const char* name) {
	void* real = g_originalGetProcAddress(module, name);

	// A name can be an ordinal instead of a string, in which case it arrives
	// as a small integer in the pointer and must not be dereferenced. The
	// first-character test after it is there so that the common case - and
	// GetProcAddress is called a great many times - costs one comparison.
	if (real != nullptr && reinterpret_cast<UInt32>(name) > 0xFFFF && name[0] == 'D' &&
	    Equals(name, "Direct3DCreate9")) {
		if (g_originalCreate9 == nullptr) {
			g_originalCreate9 = reinterpret_cast<d3d9::Direct3DCreate9Fn>(real);
			OBVR_LOG("Resolution: the game looked up Direct3DCreate9 and was handed "
			         "OBVR's instead");
		}
		return reinterpret_cast<void*>(&HookedCreate9);
	}

	return real;
}

// The second way in, for when the lookup has already happened.
//
// Oblivion caches the resolved function in a global and reads that global
// every time, so replacing what is in it is as good as being in the way of the
// lookup - right up until the moment the function is actually called.
//
// The address of that global comes from one reading of the binary, which is
// one source short of the standard this project holds addresses to. So it is
// not trusted: Direct3DCreate9 is resolved here, independently, and the global
// is only written if it already holds exactly that value. A wrong address
// cannot pass that test except by holding the right answer, in which case it
// is not wrong.
bool ReplaceCachedCreate9() {
	auto** cached = reinterpret_cast<void**>(addr::kDirect3DCreate9Pointer);
	auto* module = *reinterpret_cast<HMODULE*>(addr::kD3D9Module);

	if (*cached == nullptr || module == nullptr) {
		return false;
	}

	void* independent = GetProcAddress(module, "Direct3DCreate9");
	if (independent == nullptr || independent != *cached) {
		OBVR_LOG("Resolution: 0x%08X does not hold Direct3DCreate9, so it was left "
		         "untouched and the frame stays at the game's own size",
		         addr::kDirect3DCreate9Pointer);
		return false;
	}

	g_originalCreate9 = reinterpret_cast<d3d9::Direct3DCreate9Fn>(*cached);
	*cached = reinterpret_cast<void*>(&HookedCreate9);
	OBVR_LOG("Resolution: Direct3DCreate9 had already been looked up, and the pointer "
	         "the game kept has been replaced");
	return true;
}

}  // namespace

void SetWantedResolution(UInt32 width, UInt32 height) {
	g_wantedWidth = width;
	g_wantedHeight = height;
}

bool InstallResolutionHook() {
	if (g_originalCreate9 != nullptr || g_originalGetProcAddress != nullptr) {
		return false;
	}

	// The cached pointer first, because if it is set the lookup is already
	// behind us and hooking the lookup would catch nothing.
	if (ReplaceCachedCreate9()) {
		return true;
	}

	void* previous = nullptr;
	if (!platform::ReplaceImport(0x00400000, "KERNEL32.dll", "GetProcAddress",
	                             reinterpret_cast<void*>(&HookedGetProcAddress), &previous)) {
		return false;
	}

	g_originalGetProcAddress = reinterpret_cast<GetProcAddressFn>(previous);
	OBVR_LOG("Resolution: GetProcAddress is hooked, and d3d9.dll is %s",
	         GetModuleHandleA("d3d9.dll") != nullptr ? "already loaded"
	                                                 : "not loaded yet, which is in time");
	return true;
}

bool WasDeviceCreated(UInt32& width, UInt32& height) {
	if (!g_deviceCreated) {
		return false;
	}
	width = g_createdWidth;
	height = g_createdHeight;
	return true;
}

}  // namespace obvr::render
