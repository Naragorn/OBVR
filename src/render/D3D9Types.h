#pragma once

#include "core/Types.h"

// The parts of Direct3D 9 that OBVR calls, replicated on the same terms as
// D3D11Types.h and OpenVRTypes.h: read out of a header rather than recalled,
// and only the pieces that are used.
//
// Source: the Windows SDK's own d3d9.h and d3d9types.h, read on this machine
// at C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/shared/.
// The vtable indices below are the position of each method in
// DECLARE_INTERFACE_ order, counted from IUnknown's three.
//
// These are interface facts rather than game facts, and that distinction is
// worth more than an address: they hold for every game, every patch and every
// Direct3D 9 implementation, DXVK included. Nothing here was found in
// Oblivion.exe and nothing here can be invalidated by patching it.
//
// Why OBVR reaches through raw vtables instead of declaring the interfaces:
// IDirect3DDevice9 has 119 methods and OBVR calls four of them. Writing out
// the other 115 signatures would be 115 chances to be wrong about something
// that is never called, and each one would sit between OBVR and the methods
// it does call.

namespace obvr::render::d3d9 {

// ------------------------------------------------- IDirect3DDevice9 indices

// The end of a frame, for later. Nothing calls it yet.
constexpr UInt32 kDeviceGetBackBuffer = 18;
constexpr UInt32 kDevicePresent = 17;

// CreateTexture rather than CreateRenderTarget, and the difference decides
// whether any of this works.
//
// Both make an image that can be drawn into, and only one of them makes an
// image the compositor will accept. In DXVK, D3D9CommonTexture::CreatePrimaryImage
// adds VK_IMAGE_USAGE_SAMPLED_BIT under
//
//     if (!m_desc.IsAttachmentOnly || (!isRT && !isDS))
//       imageInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
//
// and CreateRenderTargetEx sets IsAttachmentOnly = TRUE while CreateTexture
// sets it FALSE. So a surface from CreateRenderTarget has no SAMPLED bit,
// SteamVR requires one, and usage cannot be changed after an image exists.
//
// Read through DeepWiki against doitsujin/dxvk rather than out of the checked
// out source, so it is second hand. It does not have to be trusted: OBVR reads
// the usage flags back from the image it actually made and logs them, and
// IsSubmittableImage refuses the frame if the bit is missing. The evidence
// that settles it is the first line of the next in-game log.
constexpr UInt32 kDeviceCreateTexture = 23;

// Not called. Kept next to CreateTexture because "make a render target" is
// the obvious name to reach for, and the comment above is the reason not to.
constexpr UInt32 kDeviceCreateRenderTarget = 28;

// Copies one surface into another on the GPU. This is what makes an eye's
// picture OBVR's own rather than a borrowed view of a back buffer the game
// is about to draw over.
constexpr UInt32 kDeviceStretchRect = 34;

// ------------------------------------------ IDirect3DTexture9 / Surface9

// IDirect3DTexture9 derives from IDirect3DBaseTexture9, which derives from
// IDirect3DResource9, so its own methods start at 17.
constexpr UInt32 kTextureGetSurfaceLevel = 18;

// IDirect3DSurface9 derives from IDirect3DResource9 directly.
constexpr UInt32 kSurfaceGetDesc = 12;


// Fills a surface, or part of one, with a single colour. Used once per eye
// picture, to make the margin around Oblivion's frame black rather than
// whatever the memory happened to hold.
constexpr UInt32 kDeviceColorFill = 35;

// D3DTEXTUREFILTERTYPE (d3d9types.h). NONE is the right filter only when
// there is no scaling; the moment a source rectangle and a destination
// rectangle differ in size, it has to be POINT or LINEAR.
constexpr UInt32 kTexFilterPoint = 1;
constexpr UInt32 kTexFilterLinear = 2;

// RECT (windef.h), which StretchRect and ColorFill both take.
//
// Four signed 32-bit values, and signed matters: a picture placed off the
// edge of its texture produces a negative coordinate, and an unsigned type
// would turn that into an enormous positive one rather than into an error.
// The edges are half-open, so right minus left is the width.
struct Rect {
	SInt32 left;
	SInt32 top;
	SInt32 right;
	SInt32 bottom;
};

static_assert(sizeof(Rect) == 4 * sizeof(SInt32), "RECT is four 32-bit values");

using ColorFillFn = SInt32(__stdcall*)(void* self, void* surface, const Rect* rect,
                                       UInt32 colour);

// ---------------------------------------------------------------- Constants

// D3DBACKBUFFER_TYPE_MONO
constexpr UInt32 kBackBufferTypeMono = 0;

// D3DUSAGE_RENDERTARGET (d3d9.h)
constexpr UInt32 kUsageRenderTarget = 0x00000001;

// D3DPOOL_DEFAULT (d3d9types.h). Required: a render target may not be created
// in any other pool, and the runtime returns D3DERR_INVALIDCALL if it is.
constexpr UInt32 kPoolDefault = 0;

// D3DTEXF_NONE (d3d9types.h). The right filter precisely because there is no
// scaling - source and destination are the same size, so anything else would
// be resampling a picture into itself.
constexpr UInt32 kTexFilterNone = 0;

// D3DSURFACE_DESC (d3d9types.h). Eight 32-bit fields, all enums or DWORDs, so
// this one has the same layout everywhere and needs no pointer arithmetic to
// justify.
struct SurfaceDesc {
	UInt32 format;  // D3DFORMAT
	UInt32 type;    // D3DRESOURCETYPE
	UInt32 usage;
	UInt32 pool;  // D3DPOOL
	UInt32 multiSampleType;
	UInt32 multiSampleQuality;
	UInt32 width;
	UInt32 height;
};

static_assert(sizeof(SurfaceDesc) == 8 * sizeof(UInt32),
              "D3DSURFACE_DESC must be eight 32-bit fields with no padding");


// GetTransform, which is what settles the field-of-view question by measuring
// it instead of reading documentation about it.
//
// The projection matrix Oblivion hands Direct3D contains the frustum it is
// actually rendering: for a standard perspective projection, m[0][0] is
// 1/tan(horizontal half-angle) and m[1][1] is 1/tan(vertical half-angle).
// Those two numbers are the whole answer, and they come from the game rather
// than from a wiki page written for 4:3 screens.
//
// It may not be there. Oblivion draws most of its world through shaders, and a
// game that never calls SetTransform leaves the fixed-function projection at
// identity - which is why what OBVR reads is checked for being a plausible
// projection before it is believed, and logged either way.
constexpr UInt32 kDeviceGetTransform = 45;

// IDirect3DDevice9::GetViewport. Counted the same way as the others, and
// cross-checked against two indices already known to be right: the SDK header
// lists Present five places above where it sits in the table, and SetTransform
// likewise, so GetViewport at 53 in that listing is 48 here.
//
// What it is for: OBVR now creates the frame at a size the game did not ask
// for, and the question that raises is whether Oblivion draws into all of it.
// The viewport is where that is answered - it is the rectangle of the frame
// the game is actually rendering into, and if it is smaller than the frame,
// everything OBVR copies outside it is black.
constexpr UInt32 kDeviceGetViewport = 48;

// D3DVIEWPORT9.
struct Viewport {
	UInt32 x;
	UInt32 y;
	UInt32 width;
	UInt32 height;
	float minZ;
	float maxZ;
};

// D3DTS_PROJECTION (d3d9types.h)
constexpr UInt32 kTransformProjection = 3;

// Present, for the hook that moves the submit to the end of the frame.
// Already declared above as kDevicePresent.

// D3DMATRIX (d3d9types.h): sixteen floats, row-major, m[row][column].
struct Matrix4 {
	float m[4][4];
};

static_assert(sizeof(Matrix4) == 16 * sizeof(float), "D3DMATRIX is sixteen floats");

using GetTransformFn = SInt32(__stdcall*)(void* self, UInt32 state, Matrix4* matrix);

// IDirect3DDevice9::Present. Five arguments after this, all of which OBVR
// passes straight through - it hooks this to learn when a frame is finished,
// not to change what Present does.
using PresentFn = SInt32(__stdcall*)(void* self, const Rect* source, const Rect* dest,
                                     void* destWindowOverride, const void* dirtyRegion);


// D3DPRESENT_PARAMETERS (d3d9types.h). The two fields at the front are the
// whole reason this structure is here: they are where Oblivion's frame size is
// decided, once, when the device is made.
//
// In full rather than truncated, because it is written to - a short structure
// would be one the runtime reads past the end of.
struct PresentParameters {
	UInt32 backBufferWidth;
	UInt32 backBufferHeight;
	UInt32 backBufferFormat;  // D3DFORMAT
	UInt32 backBufferCount;

	UInt32 multiSampleType;
	UInt32 multiSampleQuality;

	UInt32 swapEffect;
	void* deviceWindow;
	SInt32 windowed;
	SInt32 enableAutoDepthStencil;
	UInt32 autoDepthStencilFormat;
	UInt32 flags;

	UInt32 fullScreenRefreshRateInHz;
	UInt32 presentationInterval;
};

static_assert(sizeof(PresentParameters) == 13 * sizeof(UInt32) + sizeof(void*),
              "D3DPRESENT_PARAMETERS is thirteen 32-bit fields and one pointer");

// IDirect3D9::CreateDevice, counted from the SDK header: three IUnknown
// entries, RegisterSoftwareDevice, then eleven adapter and capability queries,
// GetAdapterMonitor, and then this.
constexpr UInt32 kFactoryCreateDevice = 16;

using GetViewportFn = SInt32(__stdcall*)(void* self, Viewport* viewport);

using Direct3DCreate9Fn = void*(__stdcall*)(UInt32 sdkVersion);

using CreateDeviceFn = SInt32(__stdcall*)(void* self, UInt32 adapter, UInt32 deviceType,
                                          void* focusWindow, UInt32 behaviourFlags,
                                          PresentParameters* parameters, void** device);

// ------------------------------------------------------------- Signatures
//
// Every one of these is __stdcall with an explicit this, which is what COM
// specifies on x86. The argument counts are copied from the header verbatim,
// because under __stdcall the callee pops the arguments: one too few or one
// too many does not produce a wrong answer, it unbalances the stack and the
// crash lands somewhere else entirely.

using GetBackBufferFn = SInt32(__stdcall*)(void* self, UInt32 swapChain, UInt32 backBuffer,
                                           UInt32 type, void** surface);

using CreateTextureFn = SInt32(__stdcall*)(void* self, UInt32 width, UInt32 height,
                                           UInt32 levels, UInt32 usage, UInt32 format,
                                           UInt32 pool, void** texture, void** sharedHandle);

// The two rectangles: null means the whole surface. Both are used now - the
// source is Oblivion's whole frame and the destination is the part of the eye
// picture that frame is entitled to at the right angular scale.
using StretchRectFn = SInt32(__stdcall*)(void* self, void* source, const Rect* sourceRect,
                                         void* dest, const Rect* destRect, UInt32 filter);

using GetSurfaceLevelFn = SInt32(__stdcall*)(void* self, UInt32 level, void** surface);

using GetDescFn = SInt32(__stdcall*)(void* self, SurfaceDesc* desc);

// Reads one method out of a COM object's vtable. Null if the object or the
// table is null, which every caller here checks - a null vtable means the
// pointer was not what it was thought to be, and calling through it would
// crash inside Oblivion with OBVR nowhere in the stack.
template <typename Fn>
inline Fn Method(void* object, UInt32 index) {
	if (object == nullptr) {
		return nullptr;
	}
	auto** vtbl = *reinterpret_cast<void***>(object);
	if (vtbl == nullptr) {
		return nullptr;
	}
	return reinterpret_cast<Fn>(vtbl[index]);
}

}  // namespace obvr::render::d3d9
