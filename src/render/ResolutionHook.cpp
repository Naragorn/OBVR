#include "render/ResolutionHook.h"

#include "core/Log.h"
#include "game/GameAddresses.h"
#include "platform/GameWindow.h"
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

// A log line per attempt would be four lines for one event, and Oblivion
// retries. Enough to see what happened, not enough to bury the rest.
UInt32 g_reportsLeft = 2;
UInt32 g_fallbacksLeft = 2;
UInt32 g_createdWidth = 0;
UInt32 g_createdHeight = 0;
UInt32 g_believedWidth = 0;
UInt32 g_believedHeight = 0;

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
//
// Two things are changed, not one, and the second is not optional.
//
// In exclusive fullscreen the back buffer is not free: it names a display
// mode, and the driver has to switch the monitor into it. A headset wants a
// square frame and no monitor has a square mode, so the switch fails and with
// it the whole device. That is not a theory - it is what happened, and DXVK
// said so in as many words:
//
//   - Windowed: false
//   Setting display mode: 3200x3200@0
//   err: D3D9: EnterFullscreenMode: Failed to change display mode
//   err: D3D9: Failed to set initial fullscreen state
//
// Oblivion retried four times and gave up, which from the outside looked like
// the game starting and vanishing.
//
// Windowed, the back buffer is just a surface. It is created at whatever size
// is asked for and the result is scaled to the window when it is presented, so
// no display mode is involved and nothing has to exist for it to be valid.
// Nothing is lost here either: in VR the monitor shows a mirror, and the
// window Oblivion already owns covers the screen.
//
// And if it still fails, the game gets its own parameters back and its device.
// A resolution that did not change is a disappointment; a game that does not
// start is not something to leave to an argument about what should work.
SInt32 __stdcall HookedCreateDevice(void* self, UInt32 adapter, UInt32 deviceType,
                                    void* focusWindow, UInt32 behaviourFlags,
                                    d3d9::PresentParameters* parameters, void** device) {
	if (parameters == nullptr) {
		return g_originalCreateDevice(self, adapter, deviceType, focusWindow,
		                              behaviourFlags, parameters, device);
	}

	const d3d9::PresentParameters asTheGameAskedFor = *parameters;

	if (g_wantedWidth != 0) {
		parameters->backBufferWidth = g_wantedWidth;
	}
	if (g_wantedHeight != 0) {
		parameters->backBufferHeight = g_wantedHeight;
	}

	const bool sizeChanged =
	    parameters->backBufferWidth != asTheGameAskedFor.backBufferWidth ||
	    parameters->backBufferHeight != asTheGameAskedFor.backBufferHeight;

	// What is deliberately NOT done here, because it was done and it burned.
	//
	// The layout probe showed the game split against itself on an eye-sized
	// frame: films and the main menu's background lay out against the INI's
	// size, everything built after the device against the real buffer, and
	// the mouse falls into the gap. The obvious cure - move the belief to its
	// source, the in-memory "iSize W:Display"/"iSize H:Display" settings, at
	// this very moment - was built, validated, and it worked; and the run
	// that proved it crashed the game in its own code (offset 0x98749, the
	// fullscreen mode path meeting a size no monitor has) and, worse, the
	// engine had already written the moved values back into the user's
	// Oblivion.ini, which then crashed every later start with or without
	// OBVR until the file was restored. A belief the engine persists on its
	// own is not a value OBVR can borrow for a session. So the game keeps
	// its own size, the flat path crops to the corner that size names, and
	// the split lives on until it can be cut somewhere the engine does not
	// write to disk.

	// Windowed whenever an eye size is in play, not merely when this call
	// changed something: an eye-sized frame is not a display mode, and that
	// stays true on the run after the game saved the eye size into its own
	// INI and asks for it as exclusive fullscreen all by itself.
	const bool wantsEyeSize = g_wantedWidth != 0 || g_wantedHeight != 0;
	if (wantsEyeSize && parameters->windowed == 0) {
		parameters->windowed = 1;

		// And the window with it, before the device is made rather than after:
		// the runtime reads the window's size while it builds the swapchain,
		// so a window resized afterwards is a swapchain already built wrong.
		//
		// This is the step that was missing the first time. Clearing the
		// fullscreen flag was necessary and correct; what it did not account
		// for is that in exclusive fullscreen Direct3D sizes the window itself
		// and Oblivion therefore never does. Windowed, the window kept the
		// 320x240 it was created with, DXVK built the swapchain at that size
		// behind a 3200x3200 back buffer, the mouse was mapped against it, and
		// the device was lost.
		//
		// Sized to the frame, not to what the game asked for. The first
		// version matched the window to the asked-for size, on the theory that
		// the mouse is mapped against the size the game believes in - which is
		// true, and is exactly why the belief itself is moved to the frame's
		// size above. Window, buffer and belief being one number is the state
		// a monitor install is in, and the state everything in the game
		// assumes.
		void* window = parameters->deviceWindow != nullptr ? parameters->deviceWindow
		                                                   : focusWindow;
		UInt32 wasWidth = 0;
		UInt32 wasHeight = 0;
		const bool sized = platform::SizeClientArea(window, parameters->backBufferWidth,
		                                            parameters->backBufferHeight,
		                                            wasWidth, wasHeight);
		if (g_reportsLeft > 0) {
			OBVR_LOG("Resolution: fullscreen cleared, and the window %s from %ux%u to %ux%u",
			         sized ? "resized" : "COULD NOT BE RESIZED, still", wasWidth, wasHeight,
			         parameters->backBufferWidth, parameters->backBufferHeight);
		}
	}

	const bool anythingChanged = sizeChanged || parameters->windowed != asTheGameAskedFor.windowed;

	if (g_reportsLeft > 0) {
		--g_reportsLeft;
		OBVR_LOG("Resolution: the game asked for %ux%u %s, and is getting %ux%u %s",
		         asTheGameAskedFor.backBufferWidth, asTheGameAskedFor.backBufferHeight,
		         asTheGameAskedFor.windowed != 0 ? "windowed" : "fullscreen",
		         parameters->backBufferWidth, parameters->backBufferHeight,
		         parameters->windowed != 0 ? "windowed" : "fullscreen");
	}

	SInt32 result = g_originalCreateDevice(self, adapter, deviceType, focusWindow,
	                                       behaviourFlags, parameters, device);

	if (result >= 0) {
		g_deviceCreated = true;
		g_createdWidth = parameters->backBufferWidth;
		g_createdHeight = parameters->backBufferHeight;

		// What the game believes its screen to be: what it asked for. The
		// flat path crops to this corner, because that is where the films
		// and the main menu's background really draw.
		g_believedWidth = asTheGameAskedFor.backBufferWidth;
		g_believedHeight = asTheGameAskedFor.backBufferHeight;
		return result;
	}

	if (!anythingChanged) {
		return result;
	}

	// Failed with OBVR's parameters. The game's own go back in - the whole
	// structure, because the runtime is allowed to have written to it - and it
	// gets the device it would have had.
	*parameters = asTheGameAskedFor;

	const SInt32 second = g_originalCreateDevice(self, adapter, deviceType, focusWindow,
	                                             behaviourFlags, parameters, device);

	if (g_fallbacksLeft > 0) {
		--g_fallbacksLeft;
		OBVR_LOG("Resolution: %ux%u was refused (0x%08X), so the game has its own %ux%u "
		         "back and %s",
		         g_wantedWidth, g_wantedHeight, static_cast<UInt32>(result),
		         asTheGameAskedFor.backBufferWidth, asTheGameAskedFor.backBufferHeight,
		         second >= 0 ? "started" : "could not make a device either way");
	}

	if (second >= 0) {
		g_deviceCreated = true;
		g_createdWidth = parameters->backBufferWidth;
		g_createdHeight = parameters->backBufferHeight;
		g_believedWidth = g_createdWidth;
		g_believedHeight = g_createdHeight;
	}
	return second;
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

bool GameBelievedSize(UInt32& width, UInt32& height) {
	if (!g_deviceCreated || g_believedWidth == 0 || g_believedHeight == 0) {
		return false;
	}
	width = g_believedWidth;
	height = g_believedHeight;
	return true;
}

}  // namespace obvr::render
