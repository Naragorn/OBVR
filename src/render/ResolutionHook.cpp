#include "render/ResolutionHook.h"

#include "core/Log.h"
#include "platform/ImportHook.h"
#include "platform/Win32Min.h"
#include "render/D3D9Types.h"

namespace obvr::render {
namespace {

UInt32 g_wantedWidth = 0;
UInt32 g_wantedHeight = 0;

d3d9::Direct3DCreate9Fn g_originalCreate9 = nullptr;
d3d9::CreateDeviceFn g_originalCreateDevice = nullptr;

bool g_deviceCreated = false;
UInt32 g_createdWidth = 0;
UInt32 g_createdHeight = 0;

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

}  // namespace

void SetWantedResolution(UInt32 width, UInt32 height) {
	g_wantedWidth = width;
	g_wantedHeight = height;
}

bool InstallResolutionHook() {
	if (g_originalCreate9 != nullptr) {
		return false;
	}

	void* previous = nullptr;
	if (!platform::ReplaceImport(0x00400000, "d3d9.dll", "Direct3DCreate9",
	                             reinterpret_cast<void*>(&HookedCreate9), &previous)) {
		return false;
	}

	g_originalCreate9 = reinterpret_cast<d3d9::Direct3DCreate9Fn>(previous);
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
