#include "render/ResolutionHook.h"

#include "core/Config.h"
#include "core/Log.h"
#include "game/GameAddresses.h"
#include "platform/GameWindow.h"
#include "render/UiScreenSize.h"
#include "platform/ImportHook.h"
#include "platform/Win32Min.h"
#include "render/D3D9Types.h"
#include "render/CreateDeviceGuard.h"

namespace obvr::render {
namespace {

UInt32 g_wantedWidth = 0;
UInt32 g_wantedHeight = 0;

d3d9::Direct3DCreate9Fn g_originalCreate9 = nullptr;
d3d9::CreateDeviceFn g_originalCreateDevice = nullptr;

// Whether the factory the game holds is an IDirect3D9Ex (the D3D9Ex probe),
// and where CreateDeviceEx sits on it: IDirect3D9's 17 methods, then
// GetAdapterModeCountEx, EnumAdapterModesEx, GetAdapterDisplayModeEx, and
// CreateDeviceEx - counted in the Windows SDK's d3d9.h.
bool g_factoryIsEx = false;
constexpr UInt32 kFactoryCreateDeviceEx = 20;

using GetProcAddressFn = void*(__stdcall*)(void* module, const char* name);
GetProcAddressFn g_originalGetProcAddress = nullptr;

bool g_deviceCreated = false;

// The slot OBVR wrote its CreateDevice into, kept so it can be looked at
// again. Patching it once is not the same as owning it: it is shared with
// anything else in the process that wants to see the device being made, and
// a run has been recorded where OBVR's factory reached the game with someone
// else's pointer in this slot. See CreateDeviceGuard.h.
void** g_createDeviceSlot = nullptr;
UInt32 g_slotRepairsLeft = 4;

// Defined below HookedCreateDevice, whose address it needs, and called from
// the GetProcAddress hook, which runs many times between the factory being
// made and the device being asked for.
void GuardTheSlot();

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

	// The 2D's screen size follows the frame - through the working copy, not
	// the settings.
	//
	// The iSize settings themselves are banned territory: rewriting them
	// closed the measured split and poisoned the user's Oblivion.ini,
	// because the engine persists them (and crashed at 0x498749, its
	// fullscreen mode path meeting a size no monitor has). What the
	// disassembly then found is that the whole 2D never reads the settings
	// directly: it reads a copy taken once at window creation - and the INI
	// is saved from the settings, never from the copy. By the time this hook
	// runs, the window and the display-mode decisions have already consumed
	// the game's own numbers, so raising the copy here reaches the menus,
	// the films and the cursor mapping, and nothing that persists or picks
	// display modes. See UiScreenSize.h and kUiScreenWidthCopy for the
	// trail.
	// Not the whole frame but a cinema window into it, when MenuAspect asks
	// for one. The full frame closed the split and handed the UI a nearly
	// square screen - everything 4:3, the main menu too narrow, said the
	// headset. At the frame width over MenuAspect the UI lays out at the
	// cinema shape, draws isotropically into the top slice of the frame, and
	// the mouse still maps against the very same numbers.
	const UiSize uiSize =
	    UiSizeForFrame(parameters->backBufferWidth, parameters->backBufferHeight,
	                   GetConfig().tracker.menuAspect);
	bool uiFollowsFrame = false;
	if (sizeChanged) {
		uiFollowsFrame = UiScreenSizeFollowsFrame(
		    GetConfig().tracker.uiFollowsFrameSize, asTheGameAskedFor.backBufferWidth,
		    asTheGameAskedFor.backBufferHeight, uiSize.width, uiSize.height);
	}

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
		// Sized to whatever the 2D believes in. With the copy raised the
		// whole game lives at the frame's size, and the window joins it;
		// with the raise refused the game stays at its own size and so does
		// the window. Window, screen-size copy and mouse mapping being one
		// number is the arrangement the game shipped against.
		const UInt32 windowWidth =
		    uiFollowsFrame ? uiSize.width : asTheGameAskedFor.backBufferWidth;
		const UInt32 windowHeight =
		    uiFollowsFrame ? uiSize.height : asTheGameAskedFor.backBufferHeight;
		void* window = parameters->deviceWindow != nullptr ? parameters->deviceWindow
		                                                   : focusWindow;
		UInt32 wasWidth = 0;
		UInt32 wasHeight = 0;
		const bool sized =
		    platform::SizeClientArea(window, windowWidth, windowHeight, wasWidth, wasHeight);
		if (g_reportsLeft > 0) {
			OBVR_LOG("Resolution: fullscreen cleared, and the window %s from %ux%u to %ux%u",
			         sized ? "resized" : "COULD NOT BE RESIZED, still", wasWidth, wasHeight,
			         windowWidth, windowHeight);
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

	// The D3D9Ex probe: with an IDirect3D9Ex factory in hand, the device is
	// made through CreateDeviceEx (IDirect3D9Ex's slot 20 in the SDK's
	// d3d9.h, after the 17 of IDirect3D9 and three Ex mode methods), with
	// no fullscreen display mode because the frame is windowed. That is the
	// device with the 9Ex semantics the experiment is about - no managed
	// pool, no device loss - handed to a game written for the other kind.
	// If the runtime refuses, the plain CreateDevice below is the fallback.
	SInt32 result = -1;
	bool madeEx = false;
	if (g_factoryIsEx) {
		using CreateDeviceExFn = SInt32(__stdcall*)(void* self, UInt32 adapter,
		                                            UInt32 deviceType, void* focusWindow,
		                                            UInt32 behaviourFlags,
		                                            d3d9::PresentParameters* parameters,
		                                            void* fullscreenMode, void** device);
		auto createEx = d3d9::Method<CreateDeviceExFn>(self, kFactoryCreateDeviceEx);
		result = createEx != nullptr
		             ? createEx(self, adapter, deviceType, focusWindow, behaviourFlags,
		                        parameters, nullptr, device)
		             : -1;
		madeEx = result >= 0;
		OBVR_LOG("D3D9Ex probe: CreateDeviceEx returned %08X - the game %s on a 9Ex device",
		         static_cast<UInt32>(result),
		         madeEx ? "now runs" : "does not get one, and the plain CreateDevice is tried");
	}
	if (!madeEx) {
		result = g_originalCreateDevice(self, adapter, deviceType, focusWindow, behaviourFlags,
		                                parameters, device);
	}

	if (result >= 0) {
		g_deviceCreated = true;
		g_createdWidth = parameters->backBufferWidth;
		g_createdHeight = parameters->backBufferHeight;

		// What the game's 2D believes its screen to be - and therefore the
		// slice of the frame its films, menus and cursor live in, which the
		// flat path and the overlay show. With the raise refused it is what
		// the game asked for, and the crops keep showing that corner.
		if (uiFollowsFrame) {
			g_believedWidth = uiSize.width;
			g_believedHeight = uiSize.height;
		} else {
			g_believedWidth = asTheGameAskedFor.backBufferWidth;
			g_believedHeight = asTheGameAskedFor.backBufferHeight;
		}
		return result;
	}

	if (!anythingChanged) {
		return result;
	}

	// Failed with OBVR's parameters. The game's own go back in - the whole
	// structure, because the runtime is allowed to have written to it - and it
	// gets the device it would have had. The screen-size copy goes back with
	// it: a copy raised to a frame that never came to be would be the split
	// with the sides swapped.
	if (uiFollowsFrame) {
		WriteUiScreenSize(uiSize.width, uiSize.height, asTheGameAskedFor.backBufferWidth,
		                  asTheGameAskedFor.backBufferHeight);
		uiFollowsFrame = false;
	}
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
	void* factory = nullptr;

	// The D3D9Ex probe (Debug.D3D9ExProbe): the factory comes from
	// Direct3DCreate9Ex instead, out of whichever d3d9.dll the game loaded -
	// Microsoft's or DXVK's, both export it. An IDirect3D9Ex is an
	// IDirect3D9 with five methods appended, so the game uses it unchanged;
	// what changes is the device made from it, see HookedCreateDevice. This
	// exists to answer the one question about the D3D9Ex route that reading
	// cannot: whether a 2006 engine tolerates the 9Ex runtime at all.
	if (GetConfig().d3d9ExProbe) {
		using Direct3DCreate9ExFn = SInt32(__stdcall*)(UInt32 sdkVersion, void** factory);
		HMODULE d3d9 = GetModuleHandleA("d3d9.dll");
		auto createEx = d3d9 != nullptr ? reinterpret_cast<Direct3DCreate9ExFn>(
		                                      GetProcAddress(d3d9, "Direct3DCreate9Ex"))
		                                : nullptr;
		void* exFactory = nullptr;
		const SInt32 made = createEx != nullptr ? createEx(sdkVersion, &exFactory) : -1;
		if (made >= 0 && exFactory != nullptr) {
			factory = exFactory;
			g_factoryIsEx = true;
			OBVR_LOG("D3D9Ex probe: the game was handed an IDirect3D9Ex factory");
		} else {
			OBVR_LOG("D3D9Ex probe: Direct3DCreate9Ex %s (%08X), so the plain factory is used",
			         createEx != nullptr ? "failed" : "is not exported by this d3d9.dll",
			         static_cast<UInt32>(made));
		}
	}

	if (factory == nullptr) {
		factory = g_originalCreate9 != nullptr ? g_originalCreate9(sdkVersion) : nullptr;
	}
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
	g_createDeviceSlot = slot;

	DWORD ignored = 0;
	VirtualProtect(slot, sizeof(void*), protection, &ignored);

	OBVR_LOG("Resolution: CreateDevice hooked at factory entry %u",
	         d3d9::kFactoryCreateDevice);
	return factory;
}

// Names the module a pointer came out of, so the log can say who, not just
// that. Nothing is done with the name beyond writing it down and comparing it
// with OBVR's own.
bool ModuleOf(void* address, HMODULE& module) {
	module = nullptr;
	const DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
	                    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
	return GetModuleHandleExA(flags, reinterpret_cast<const char*>(address), &module) != 0 &&
	       module != nullptr;
}

// Watches the one slot the frame size hangs on, for as long as the device is
// still to come.
//
// This is called from the GetProcAddress hook rather than on a timer, because
// that is where the calls are: between the factory being made and the device
// being asked for, DXVK resolves most of Vulkan by name, and Oblivion resolves
// the rest of what it needs. The cost of being here is one pointer comparison
// per lookup, which is why the quiet case is written out in full below instead
// of being left to the decision - naming a module costs a system call, and
// that price is only paid once something is actually wrong.
void GuardTheSlot() {
	if (g_createDeviceSlot == nullptr || g_deviceCreated ||
	    *g_createDeviceSlot == reinterpret_cast<void*>(&HookedCreateDevice)) {
		return;
	}

	void* const foreign = *g_createDeviceSlot;

	HMODULE foreignModule = nullptr;
	HMODULE ownModule = nullptr;
	const bool foreignNamed = ModuleOf(foreign, foreignModule);
	const bool ownNamed = ModuleOf(reinterpret_cast<void*>(&HookedCreateDevice), ownModule);

	SlotGuardInput input;
	input.slotKnown = true;
	input.deviceCreated = false;
	input.slotIsOurs = false;
	input.foreignIsOurModule = foreignNamed && ownNamed && foreignModule == ownModule;
	input.repairsLeft = g_slotRepairsLeft;

	const SlotGuardDecision decision = GuardCreateDeviceSlot(input);
	if (!decision.repair) {
		return;
	}

	char name[260];
	name[0] = '\0';
	if (foreignNamed) {
		GetModuleFileNameA(foreignModule, name, sizeof(name));
	}

	DWORD protection = 0;
	if (!VirtualProtect(g_createDeviceSlot, sizeof(void*), PAGE_READWRITE, &protection)) {
		OBVR_LOG("Resolution: the slot changed hands and could not be made writable "
		         "again, so the frame stays at the game's own size");
		g_slotRepairsLeft = 0;
		return;
	}

	if (decision.adoptForeign) {
		g_originalCreateDevice = reinterpret_cast<d3d9::CreateDeviceFn>(foreign);
	}
	*g_createDeviceSlot = reinterpret_cast<void*>(&HookedCreateDevice);

	DWORD ignored = 0;
	VirtualProtect(g_createDeviceSlot, sizeof(void*), protection, &ignored);

	--g_slotRepairsLeft;

	if (decision.report) {
		OBVR_LOG("Resolution: CreateDevice had been taken over by %08X (%s) before the "
		         "device was made, and OBVR is back in front of %s",
		         reinterpret_cast<UInt32>(foreign),
		         name[0] != '\0' ? name : "a module that could not be named",
		         decision.adoptForeign ? "it" : "the original, that pointer being OBVR's own");
	}
}

// The way in, because Oblivion has no d3d9 import to replace.
//
// It loads the library by hand and looks the function up by name, so the name
// is where OBVR gets in the way. Every other lookup in the process passes
// through unchanged and unexamined beyond one character.
void* __stdcall HookedGetProcAddress(void* module, const char* name) {
	void* real = g_originalGetProcAddress(module, name);

	// The slot is shared property, and this is the only moment OBVR is
	// running between patching it and the device being built.
	GuardTheSlot();

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
