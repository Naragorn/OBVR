// Pins the Direct3D 9 vtable indices and constants OBVR reaches through.
//
// Every number in D3D9Types.h was read out of the Windows SDK's own d3d9.h on
// the machine this was built on, by counting DECLARE_INTERFACE_ entries from
// IUnknown's three. That is a transcription, and a transcription is exactly
// the kind of thing that is right when written and wrong after an edit two
// months later.
//
// What makes these worth a test is how they fail. Calling through the wrong
// slot does not report an error: it calls a different method with this
// method's arguments. Under __stdcall the callee pops what it thinks it was
// given, so an index that is off by one unbalances the stack and the crash
// surfaces somewhere with no connection to Direct3D at all. There is no log
// line to find and nothing pointing back here.
//
// No Windows API, so this builds and runs on Linux as well.

#include <cstddef>
#include <cstdio>

#include "render/D3D9Types.h"

namespace d3d9 = obvr::render::d3d9;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

void CheckIndex(UInt32 actual, UInt32 expected, const char* what) {
	if (actual == expected) {
		std::printf("  ok    %s is at %u\n", what, expected);
		return;
	}
	std::printf("  FAIL  %s should be at %u, is at %u\n", what, expected, actual);
	++g_failures;
}

void TestDeviceIndices() {
	std::printf("IDirect3DDevice9 vtable\n");

	using namespace obvr::render::d3d9;

	// Counted from the SDK header: three IUnknown entries, then
	// TestCooperativeLevel, GetAvailableTextureMem, EvictManagedResources,
	// GetDirect3D, GetDeviceCaps, GetDisplayMode, GetCreationParameters, the
	// three cursor methods, CreateAdditionalSwapChain, GetSwapChain,
	// GetNumberOfSwapChains, Reset, and then Present.
	CheckIndex(kDevicePresent, 17, "Present");
	CheckIndex(kDeviceGetBackBuffer, 18, "GetBackBuffer");

	// After GetBackBuffer: GetRasterStatus, SetDialogBoxMode, SetGammaRamp,
	// GetGammaRamp, and then the create methods begin.
	CheckIndex(kDeviceCreateTexture, 23, "CreateTexture");
	CheckIndex(kDeviceCreateRenderTarget, 28, "CreateRenderTarget");
	CheckIndex(kDeviceStretchRect, 34, "StretchRect");

	// The gaps, spelled out as arithmetic so a wrong one shows up as a wrong
	// count rather than as two numbers that each look plausible on their own.
	//
	// Between CreateTexture and CreateRenderTarget: CreateVolumeTexture,
	// CreateCubeTexture, CreateVertexBuffer, CreateIndexBuffer.
	Check(kDeviceCreateRenderTarget - kDeviceCreateTexture == 5,
	      "four entries stand between CreateTexture and CreateRenderTarget");

	// Between CreateRenderTarget and StretchRect: CreateDepthStencilSurface,
	// UpdateSurface, UpdateTexture, GetRenderTargetData, GetFrontBufferData.
	Check(kDeviceStretchRect - kDeviceCreateRenderTarget == 6,
	      "five entries stand between CreateRenderTarget and StretchRect");
}

void TestResourceIndices() {
	std::printf("IDirect3DTexture9 and IDirect3DSurface9\n");

	using namespace obvr::render::d3d9;

	// IDirect3DTexture9 derives from IDirect3DBaseTexture9, which derives
	// from IDirect3DResource9. Eight resource methods after IUnknown's three,
	// six base-texture ones, then GetLevelDesc at 17 and GetSurfaceLevel at 18.
	CheckIndex(kTextureGetSurfaceLevel, 18, "GetSurfaceLevel");

	// IDirect3DSurface9 derives from IDirect3DResource9 directly, so it skips
	// the six base-texture entries: GetContainer at 11, GetDesc at 12.
	CheckIndex(kSurfaceGetDesc, 12, "GetDesc");

	// The same number on two unrelated interfaces, and that coincidence is
	// precisely why they are written out separately. GetSurfaceLevel on a
	// texture and GetBackBuffer on a device agree by accident, and one
	// constant serving both would keep working until either of them moved.
	Check(kTextureGetSurfaceLevel == kDeviceGetBackBuffer,
	      "GetSurfaceLevel and GetBackBuffer share an index by coincidence, not by rule");
}

void TestConstants() {
	std::printf("Constants\n");

	using namespace obvr::render::d3d9;

	Check(kBackBufferTypeMono == 0, "D3DBACKBUFFER_TYPE_MONO is 0");
	Check(kUsageRenderTarget == 0x00000001, "D3DUSAGE_RENDERTARGET is 0x1");

	// Not a preference. A render target may be created in no other pool, and
	// the runtime answers D3DERR_INVALIDCALL rather than explaining why.
	Check(kPoolDefault == 0, "D3DPOOL_DEFAULT is 0");

	// D3DTEXF_NONE, and the right filter here precisely because there is no
	// scaling: the copy is the same size as the back buffer it comes from.
	Check(kTexFilterNone == 0, "D3DTEXF_NONE is 0");
}

void TestSurfaceDesc() {
	std::printf("D3DSURFACE_DESC layout\n");

	using d3d9::SurfaceDesc;

	// Eight 32-bit fields in the order the header declares them. Format,
	// Width and Height are the three OBVR reads, and two of those are last -
	// so every field before them has to be the right size, or the eye copies
	// are created at some other resolution with nothing saying why.
	Check(offsetof(SurfaceDesc, format) == 0, "Format is first");
	Check(offsetof(SurfaceDesc, type) == 4, "then Type");
	Check(offsetof(SurfaceDesc, usage) == 8, "then Usage");
	Check(offsetof(SurfaceDesc, pool) == 12, "then Pool");
	Check(offsetof(SurfaceDesc, multiSampleType) == 16, "then MultiSampleType");
	Check(offsetof(SurfaceDesc, multiSampleQuality) == 20, "then MultiSampleQuality");
	Check(offsetof(SurfaceDesc, width) == 24, "then Width");
	Check(offsetof(SurfaceDesc, height) == 28, "and Height last");
	Check(sizeof(SurfaceDesc) == 32, "and the whole thing is 32 bytes");
}

// A stand-in for a COM object: a pointer to a table, which is how every COM
// object begins. The entries are not real functions, because what is checked
// here is which slot gets read and not what happens after it.
struct FakeObject {
	void** vtbl;
};

void TestMethodLookup() {
	std::printf("Reading a method out of a vtable\n");

	using d3d9::Method;
	using Fn = void (*)();

	// Distinguishable entries, so a lookup that lands one slot off does not
	// accidentally match the slot it should have hit.
	void* table[40];
	for (int i = 0; i < 40; ++i) {
		table[i] = static_cast<void*>(&table[i]);
	}

	FakeObject object;
	object.vtbl = table;

	Check(reinterpret_cast<void*>(Method<Fn>(&object, d3d9::kDeviceGetBackBuffer)) ==
	          table[18],
	      "the GetBackBuffer index reads entry 18 and no other");
	Check(reinterpret_cast<void*>(Method<Fn>(&object, d3d9::kDeviceCreateTexture)) ==
	          table[23],
	      "the CreateTexture index reads entry 23");
	Check(reinterpret_cast<void*>(Method<Fn>(&object, d3d9::kDeviceStretchRect)) ==
	          table[34],
	      "the StretchRect index reads entry 34");

	// The two ways the object itself can be wrong, and both are reachable:
	// the renderer global is null until the game builds one, and a device
	// read from the wrong offset points at something that is not a COM object
	// at all. Calling through either would crash inside Oblivion with OBVR
	// nowhere in the stack.
	Check(Method<Fn>(nullptr, 18) == nullptr, "a null object yields no method");

	FakeObject empty;
	empty.vtbl = nullptr;
	Check(Method<Fn>(&empty, 18) == nullptr, "and neither does a null vtable");
}

}  // namespace

int main() {
	std::printf("OBVR Direct3D 9 types test\n\n");

	TestDeviceIndices();
	std::printf("\n");
	TestResourceIndices();
	std::printf("\n");
	TestConstants();
	std::printf("\n");
	TestSurfaceDesc();
	std::printf("\n");
	TestMethodLookup();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
