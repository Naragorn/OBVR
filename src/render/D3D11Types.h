#pragma once

#include "core/Types.h"

// Minimal, binary-compatible replica of the parts of Direct3D 11 that OBVR
// needs, on the same terms as src/vr/OpenVRTypes.h and src/obse/PluginInterface.h.
//
// Sources, all read rather than recalled:
//   * method order and struct layouts from Wine's include/d3d11.idl,
//     include/dxgicommon.idl, include/dxgiformat.idl and include/d3dcommon.idl
//   * the D3D11CreateDevice signature from its Microsoft Learn reference page
//
// Why replicate instead of including d3d11.h: the same reason as everywhere
// else here. Out of a very large header OBVR needs one function, three
// interfaces and two structs, and the SDK-free verification build on Linux
// has no d3d11.h to include.
//
// Written in the C style COM presents rather than as C++ classes with virtual
// methods. A hand-written C++ vtable would depend on the ABI of whichever
// compiler builds OBVR; a struct of function pointers taking an explicit this
// depends only on the COM ABI, which is fixed. That is the same argument that
// put OBVR on OpenVR's "FnTable:" interfaces rather than its C++ ones.
//
// Every method here is __stdcall, which is what COM specifies on x86. There
// is no second calling convention in this file, so no equivalent of the trap
// documented at the top of OpenVRTypes.h.

namespace obvr::render::d3d11 {

// HRESULT. Not a typedef from a header OBVR does not include; it is a signed
// 32-bit value, and negative means failure - that single rule is all of the
// convention OBVR uses.
using ResultCode = SInt32;

inline bool Failed(ResultCode result) { return result < 0; }

// ------------------------------------------------------------- Constants

// D3D11_SDK_VERSION (d3d11.idl: const UINT D3D11_SDK_VERSION = 7)
//
// Passing the wrong number here does not produce a subtly wrong device, it
// produces no device at all - which is the good kind of failure, but only if
// the number is right for the reason that it was read.
constexpr UInt32 kSdkVersion = 7;

// D3D_DRIVER_TYPE (d3dcommon.idl), counted from Unknown = 0
constexpr int kDriverTypeHardware = 1;
constexpr int kDriverTypeWarp = 5;

// D3D11_USAGE (d3d11.idl), counted from Default = 0
constexpr UInt32 kUsageDefault = 0;

// D3D11_BIND_FLAG (d3d11.idl)
constexpr UInt32 kBindShaderResource = 0x0008;
constexpr UInt32 kBindRenderTarget = 0x0020;

// DXGI_FORMAT_R8G8B8A8_UNORM (dxgiformat.idl, = 0x1c)
//
// One of the three formats a D3D9 texture may be shared into D3D11 as, which
// is why it is the one to get working now: choosing it here means the format
// does not have to change when 0.1.0 stops generating the picture and starts
// taking Oblivion's.
constexpr UInt32 kFormatR8G8B8A8Unorm = 28;

// ---------------------------------------------------------------- Structs

// DXGI_SAMPLE_DESC (dxgicommon.idl)
struct SampleDesc {
	UInt32 count;
	UInt32 quality;
};

// D3D11_TEXTURE2D_DESC (d3d11.idl)
struct Texture2DDesc {
	UInt32 width;
	UInt32 height;
	UInt32 mipLevels;
	UInt32 arraySize;
	UInt32 format;  // DXGI_FORMAT
	SampleDesc sampleDesc;
	UInt32 usage;  // D3D11_USAGE
	UInt32 bindFlags;
	UInt32 cpuAccessFlags;
	UInt32 miscFlags;
};

// D3D11_SUBRESOURCE_DATA (d3d11.idl)
struct SubresourceData {
	const void* sysMem;
	UInt32 sysMemPitch;
	UInt32 sysMemSlicePitch;
};

// Eleven 32-bit fields and no pointer, so this one is the same size on either
// architecture and a plain count says what is meant.
static_assert(sizeof(Texture2DDesc) == 11 * sizeof(UInt32),
              "D3D11_TEXTURE2D_DESC must be eleven 32-bit fields with no padding");

// This one does hold a pointer, so it is written as a sum for the reason
// spelled out in OpenVRTypes.h: the DLL that ships is 32 bit and the tests
// that check it are not.
static_assert(sizeof(SubresourceData) == sizeof(void*) + 2 * sizeof(UInt32),
              "D3D11_SUBRESOURCE_DATA must pack without padding");

// ------------------------------------------------------------- Interfaces

// Every COM object begins with a pointer to its table of methods, and every
// table begins with IUnknown's three. That is enough to release anything
// without knowing what it is.
struct UnknownVtbl {
	ResultCode(__stdcall* QueryInterface)(void* self, const void* iid, void** out);
	UInt32(__stdcall* AddRef)(void* self);
	UInt32(__stdcall* Release)(void* self);
};

struct Unknown {
	const UnknownVtbl* vtbl;
};

// Releases a COM object and clears the caller's pointer.
//
// Takes void*& so that it works for every interface here without a cast at
// each call site, and clears the pointer so a double release is impossible
// rather than merely unlikely - the failure mode being a crash at shutdown,
// which users report as "it crashes when I quit" and nobody can place.
inline void Release(void*& object) {
	if (object == nullptr) {
		return;
	}
	auto* unknown = static_cast<Unknown*>(object);
	object = nullptr;
	if (unknown->vtbl != nullptr && unknown->vtbl->Release != nullptr) {
		unknown->vtbl->Release(unknown);
	}
}

struct Device;
struct Texture2D {
	const UnknownVtbl* vtbl;
};

// Excerpt from ID3D11Device (d3d11.idl). It derives from IUnknown, so its own
// methods start at index 3:
//
//    0 QueryInterface     3 CreateBuffer        6 CreateTexture3D
//    1 AddRef             4 CreateTexture1D     7 CreateShaderResourceView
//    2 Release            5 CreateTexture2D     8 CreateUnorderedAccessView
//
// The entries before CreateTexture2D are the ones a listing that forgets
// IUnknown gets wrong, and it gets them wrong by exactly three - which lands
// on CreateShaderResourceView with a completely different argument list.
struct DeviceVtbl {
	ResultCode(__stdcall* QueryInterface)(Device* self, const void* iid, void** out);
	UInt32(__stdcall* AddRef)(Device* self);
	UInt32(__stdcall* Release)(Device* self);

	void* createBuffer;
	void* createTexture1D;

	ResultCode(__stdcall* CreateTexture2D)(Device* self, const Texture2DDesc* desc,
	                                       const SubresourceData* initialData,
	                                       Texture2D** texture);
};

struct Device {
	const DeviceVtbl* vtbl;
};

// ------------------------------------------------------------ DLL export

// D3D11CreateDevice, from its Microsoft Learn reference page. Only the
// arguments OBVR passes are typed; the rest are pointers it always sets to
// null, and naming their types would mean replicating IDXGIAdapter and
// D3D_FEATURE_LEVEL for no gain.
//
// Loaded through LoadLibrary rather than imported, exactly as openvr_api.dll
// is. d3d11.dll has shipped with Windows since 7 and would be a safe import,
// but a runtime load keeps the import list at kernel32, msvcrt and user32 -
// which is what lets the SDK-free build on Linux keep working, and what lets
// OBVR fail with a log line instead of a loader error if it is ever wrong.
using CreateDeviceFn = ResultCode(__stdcall*)(void* adapter, int driverType, void* software,
                                              UInt32 flags, const int* featureLevels,
                                              UInt32 featureLevelCount, UInt32 sdkVersion,
                                              Device** device, int* featureLevel,
                                              void** immediateContext);

}  // namespace obvr::render::d3d11
