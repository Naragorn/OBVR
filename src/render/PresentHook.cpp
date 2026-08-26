#include "render/PresentHook.h"

#include "core/Log.h"
#include "platform/Win32Min.h"
#include "render/D3D9Types.h"

namespace obvr::render {
namespace {

// The one entry that was replaced, and everything needed to put it back.
//
// File-scope rather than a class, because a method table belongs to a class
// and not to an object: there is one of these in the process whatever OBVR
// does, so pretending otherwise by allowing two instances would only make the
// duplicate-install case harder to refuse.
void** g_vtable = nullptr;
d3d9::PresentFn g_original = nullptr;
FrameEndCallback g_callback = nullptr;

// Sits in Oblivion's method table where Present used to be.
//
// The callback first, then the original with the arguments untouched. Before
// Present runs, the back buffer holds the frame that is about to be shown -
// which is precisely the picture the compositor should be given, and precisely
// what the camera hook could not reach.
SInt32 __stdcall HookedPresent(void* self, const d3d9::Rect* source, const d3d9::Rect* dest,
                               void* destWindowOverride, const void* dirtyRegion) {
	if (g_callback != nullptr) {
		g_callback();
	}

	// Not conditional. If the original is somehow missing, returning a made-up
	// success would leave the game with no picture at all and no error - so
	// this is the one place where doing nothing is worse than crashing.
	return g_original(self, source, dest, destWindowOverride, dirtyRegion);
}

// Makes one table entry writable, changes it, and puts the protection back.
//
// The table lives in the runtime's read-only data, so writing to it without
// this raises an access violation - inside DXVK, on Oblivion's thread, with
// nothing pointing back here.
bool WriteEntry(void** vtable, UInt32 index, void* value) {
	void** slot = &vtable[index];

	DWORD previous = 0;
	if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &previous)) {
		return false;
	}

	*slot = value;

	DWORD ignored = 0;
	VirtualProtect(slot, sizeof(void*), previous, &ignored);
	return true;
}

}  // namespace

bool LooksLikeVtable(void* const* vtable, UInt32 entries) {
	if (vtable == nullptr) {
		return false;
	}

	// Every entry up to the one being replaced has to be a pointer to
	// something. A table of function pointers has no holes: a null in the
	// middle means this is not a method table, it is some other structure that
	// happens to start with a pointer.
	//
	// This does not prove it is Direct3D's table, and nothing can from inside
	// the process. It rejects the cases that are cheap to reject.
	for (UInt32 index = 0; index < entries; ++index) {
		if (vtable[index] == nullptr) {
			return false;
		}
	}

	return true;
}

bool InstallPresentHook(void* gameDevice, FrameEndCallback callback) {
	if (gameDevice == nullptr || callback == nullptr) {
		return false;
	}

	// Refused rather than chained. A second install would put the hook in
	// front of the hook, and the callback would run twice per frame - which
	// submits twice, and the second submit has no WaitGetPoses in front of it.
	if (g_vtable != nullptr) {
		OBVR_LOG("Present: already hooked, so the second request was refused");
		return false;
	}

	auto** vtable = *reinterpret_cast<void***>(gameDevice);
	if (!LooksLikeVtable(vtable, d3d9::kDevicePresent + 1)) {
		OBVR_LOG("Present: the device's method table does not look like one, so nothing "
		         "was changed");
		return false;
	}

	auto original = reinterpret_cast<d3d9::PresentFn>(vtable[d3d9::kDevicePresent]);

	if (!WriteEntry(vtable, d3d9::kDevicePresent, reinterpret_cast<void*>(&HookedPresent))) {
		OBVR_LOG("Present: entry %u could not be made writable, so the submit stays at the "
		         "start of the frame",
		         d3d9::kDevicePresent);
		return false;
	}

	g_vtable = vtable;
	g_original = original;
	g_callback = callback;

	OBVR_LOG("Present: hooked at table entry %u, original at %08X - the picture submitted "
	         "is now the one just drawn",
	         d3d9::kDevicePresent, reinterpret_cast<UInt32>(original));
	return true;
}

void RemovePresentHook() {
	if (g_vtable == nullptr) {
		return;
	}

	WriteEntry(g_vtable, d3d9::kDevicePresent, reinterpret_cast<void*>(g_original));

	g_vtable = nullptr;
	g_original = nullptr;
	g_callback = nullptr;
}

bool IsPresentHooked() { return g_vtable != nullptr; }

}  // namespace obvr::render
