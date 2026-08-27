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

// D3DTS_VIEW (d3d9types.h, line 331) and D3DTS_WORLD, which the macro at
// line 344 defines as world matrix index 0 plus 256. Read at the moment of
// the first redirected draw: the interface's vertices are untransformed
// (D3DFVF_XYZ), so these three matrices decide where its pixel coordinates
// land - and whether they land at all.
constexpr UInt32 kTransformView = 2;
constexpr UInt32 kTransformWorld = 256;

// ------------------------------------------------- The 2D layer's redirect
//
// Counted the same way as every other index here, and cross-checked against
// two neighbours already known to be right: the SDK lists StretchRect and
// ColorFill one apart at 35/36, which sit at 34/35 in the table, and
// SetRenderTarget two entries later at 38 is therefore 37 - the number
// HANDOFF's route B section named from the same counting.
//
// What they are for: the pass that draws Oblivion's HUD and menus begins its
// own render target group from inside, so redirecting it to a texture of
// OBVR's own cannot be done around the call - it has to happen at the one
// method every Gamebryo wrapper ends at. That is an API fact: there is no
// other way to change the colour target in Direct3D 9.
constexpr UInt32 kDeviceSetRenderTarget = 37;
constexpr UInt32 kDeviceGetRenderTarget = 38;

// SetRenderState / GetRenderState, listed at 58/59 in the SDK header.
//
// Needed for one thing: destination alpha that means coverage. The UI blends
// SRCALPHA/INVSRCALPHA, and without a separate alpha function the alpha
// written to the target is alpha squared - close, and slightly too
// transparent everywhere. ONE/INVSRCALPHA on the alpha side is the correct
// over-operator. The previous values are read first and put back afterwards.
constexpr UInt32 kDeviceSetRenderState = 57;
constexpr UInt32 kDeviceGetRenderState = 58;

// CreateStateBlock, and the two methods of what it returns.
//
// The header declares CreateStateBlock immediately after GetRenderState and
// before BeginStateBlock, which puts it at 59 - and kDeviceGetTexture = 64
// below is the independent check on that count, since the four entries in
// between are BeginStateBlock, EndStateBlock, SetClipStatus and GetClipStatus.
//
// IDirect3DStateBlock9 derives from IUnknown and declares GetDevice, Capture
// and Apply, so those are 3, 4 and 5.
//
// D3DSBT_ALL is 1. Creating the block already captures, so the first Capture
// after creation is redundant and harmless - and the code below relies on
// that rather than on the block being empty.
constexpr UInt32 kDeviceCreateStateBlock = 59;
constexpr UInt32 kStateBlockCapture = 4;
constexpr UInt32 kStateBlockApply = 5;
constexpr UInt32 kStateBlockTypeAll = 1;

// D3DRENDERSTATETYPE values (d3d9types.h, lines 450-453 and 364).
constexpr UInt32 kRenderStateSeparateAlphaBlendEnable = 206;
constexpr UInt32 kRenderStateSrcBlendAlpha = 207;
constexpr UInt32 kRenderStateDestBlendAlpha = 208;
constexpr UInt32 kRenderStateBlendOpAlpha = 209;

// D3DBLEND / D3DBLENDOP values (d3d9types.h, lines 230, 234, 256).
constexpr UInt32 kBlendOne = 2;
constexpr UInt32 kBlendInvSrcAlpha = 6;
constexpr UInt32 kBlendOpAdd = 1;

// D3DRS_COLORWRITEENABLE (d3d9types.h, line 416) and its channel bits
// (lines 486-489). A game whose back buffer is X8R8G8B8 has no alpha channel
// to write, so it may legitimately leave the alpha bit off - and everything
// it draws into OBVR's A8R8G8B8 texture then lands with alpha zero, which an
// overlay renders as nothing at all.
constexpr UInt32 kRenderStateColorWriteEnable = 168;
constexpr UInt32 kColorWriteAlpha = 0x8;
constexpr UInt32 kColorWriteAll = 0xF;

// Reading a render target back to the CPU, for one diagnostic: the HUD
// texture arrived in the headset as nothing while the red probe square
// showed, so the question became what the texture actually holds - and that
// is a question about bytes, which only a readback answers. All counted from
// the same DECLARE_INTERFACE_ listing as everything above.
constexpr UInt32 kDeviceGetRenderTargetData = 32;
constexpr UInt32 kDeviceCreateOffscreenPlainSurface = 36;
constexpr UInt32 kSurfaceLockRect = 13;
constexpr UInt32 kSurfaceUnlockRect = 14;

// D3DPOOL_SYSTEMMEM (d3d9types.h, line 1525) - the pool GetRenderTargetData
// copies into - and D3DLOCK_READONLY (line 1695).
constexpr UInt32 kPoolSystemMem = 2;
constexpr UInt32 kLockReadOnly = 0x10;

// D3DLOCKED_RECT (d3d9types.h, line 1760): the pitch first, then the bits.
struct LockedRect {
	SInt32 pitch;
	void* bits;
};

// The four ways Direct3D 9 draws anything, counted for one diagnostic: the
// redirected 2D pass produced a texture of nothing, and whether it issued
// draws at all - and at which targets - is what separates "the HUD is drawn
// somewhere else entirely" from "it drew and the pixels went astray".
constexpr UInt32 kDeviceDrawPrimitive = 81;
constexpr UInt32 kDeviceDrawIndexedPrimitive = 82;
constexpr UInt32 kDeviceDrawPrimitiveUP = 83;
constexpr UInt32 kDeviceDrawIndexedPrimitiveUP = 84;

using DrawPrimitiveFn = SInt32(__stdcall*)(void* self, UInt32 type, UInt32 startVertex,
                                           UInt32 primitiveCount);
using DrawIndexedPrimitiveFn = SInt32(__stdcall*)(void* self, UInt32 type,
                                                  SInt32 baseVertexIndex,
                                                  UInt32 minVertexIndex, UInt32 numVertices,
                                                  UInt32 startIndex, UInt32 primCount);
using DrawPrimitiveUPFn = SInt32(__stdcall*)(void* self, UInt32 type, UInt32 primitiveCount,
                                             const void* vertexData, UInt32 stride);
using DrawIndexedPrimitiveUPFn = SInt32(__stdcall*)(void* self, UInt32 type,
                                                    UInt32 minVertexIndex,
                                                    UInt32 numVertices, UInt32 primitiveCount,
                                                    const void* indexData, UInt32 indexFormat,
                                                    const void* vertexData, UInt32 stride);

using GetRenderTargetDataFn = SInt32(__stdcall*)(void* self, void* renderTarget,
                                                 void* destSurface);
using CreateOffscreenPlainSurfaceFn = SInt32(__stdcall*)(void* self, UInt32 width,
                                                         UInt32 height, UInt32 format,
                                                         UInt32 pool, void** surface,
                                                         void** sharedHandle);
using LockRectFn = SInt32(__stdcall*)(void* self, LockedRect* locked, const Rect* rect,
                                      UInt32 flags);
using UnlockRectFn = SInt32(__stdcall*)(void* self);

// Clear (43), counted from the same DECLARE_INTERFACE_ listing as every
// index above, cross-checked against the verified GetRenderTarget (38) and
// GetTransform (45) with the depth stencil pair and BeginScene/EndScene in
// between. What it is for: the smallest write that goes through the render
// target *binding* rather than to a surface named directly. ColorFill proved
// the texture's surface is writable and reaches the compositor; twenty-two
// successful draws through the binding still left the texture empty, and a
// Clear uses exactly the binding the draws use.
constexpr UInt32 kDeviceClear = 43;

// D3DCLEAR_TARGET (d3d9types.h, line 206). The rects share Rect's four-long
// layout, and the calls here pass none anyway.
constexpr UInt32 kClearTarget = 0x1;

// D3DCLEAR_ZBUFFER / D3DCLEAR_STENCIL (d3d9types.h, lines 207-208), and
// D3DFMT_D24S8 (line 1422) - the depth format whose stencil planes may be
// cleared alongside; the stencil flag on a stencil-less surface fails the
// whole clear instead of part of it.
constexpr UInt32 kClearZBuffer = 0x2;
constexpr UInt32 kClearStencil = 0x4;
constexpr UInt32 kFormatD24S8 = 75;

// GetDepthStencilSurface (40), counted from the same listing, between the
// verified GetRenderTarget (38) and Clear (43) with SetDepthStencilSurface
// and the scene bracket in between. What it is for: the redirected pass's
// draws run with the z test on, and whether they test against anything - and
// against what - is a question about the surface the device actually holds.
constexpr UInt32 kDeviceGetDepthStencilSurface = 40;

using GetDepthStencilSurfaceFn = SInt32(__stdcall*)(void* self, void** surface);

using ClearFn = SInt32(__stdcall*)(void* self, UInt32 count, const Rect* rects,
                                   UInt32 flags, UInt32 color, float z, UInt32 stencil);

// D3DRS_SRCBLEND / D3DRS_DESTBLEND (d3d9types.h, lines 357-358) and
// D3DRS_STENCILENABLE (line 373): the states sampled at the moment of the
// first redirected draw. The pass-entry snapshot showed every rejection
// state off, but the pass entry is not the draw - what the state is when
// the game actually draws is a separate measurement.
constexpr UInt32 kRenderStateSrcBlend = 19;
constexpr UInt32 kRenderStateDestBlend = 20;
constexpr UInt32 kRenderStateStencilEnable = 52;

// D3DRS_CULLMODE (line 359), D3DRS_TEXTUREFACTOR (line 381) and
// D3DRS_LIGHTING (line 391): the remaining silent rejectors a fixed-function
// draw can run into. Culled winding produces nothing; lighting against no
// lights produces black; a texture factor of zero alpha produces pixels the
// blend leaves invisible.
constexpr UInt32 kRenderStateCullMode = 22;
constexpr UInt32 kRenderStateTextureFactor = 60;
constexpr UInt32 kRenderStateLighting = 137;

// GetTexture (64) and GetTextureStageState (66), counted from the same
// listing between the verified state pair (57/58) and the verified draw
// quartet (81-84), with the state block and clip status methods between.
// The stage constants are D3DTSS_COLOROP through D3DTSS_ALPHAARG2
// (d3d9types.h, lines 497-502).
constexpr UInt32 kDeviceGetTexture = 64;
constexpr UInt32 kDeviceGetTextureStageState = 66;
constexpr UInt32 kStageColorOp = 1;
constexpr UInt32 kStageColorArg1 = 2;
constexpr UInt32 kStageColorArg2 = 3;
constexpr UInt32 kStageAlphaOp = 4;
constexpr UInt32 kStageAlphaArg1 = 5;
constexpr UInt32 kStageAlphaArg2 = 6;

using GetTextureFn = SInt32(__stdcall*)(void* self, UInt32 stage, void** texture);
using GetTextureStageStateFn = SInt32(__stdcall*)(void* self, UInt32 stage, UInt32 type,
                                                  UInt32* value);

// GetFVF (90), GetVertexShader (93) and GetPixelShader (108), counted from
// the same listing, anchored on the verified draw quartet at 81-84 with
// ProcessVertices and the declaration methods between. They say what kind
// of pipeline the first redirected draw ran on - fixed function or shaders -
// which decides where to look when its pixels do not arrive.
constexpr UInt32 kDeviceGetFVF = 90;
constexpr UInt32 kDeviceGetVertexShader = 93;
constexpr UInt32 kDeviceGetPixelShader = 108;

using GetFVFFn = SInt32(__stdcall*)(void* self, UInt32* fvf);
using GetShaderFn = SInt32(__stdcall*)(void* self, void** shader);

// D3DFMT_A8R8G8B8 (d3d9types.h, line 1378). The back buffer is X8R8G8B8 - no
// alpha, because a screen has no use for one. The 2D layer's own texture is
// the opposite case: the alpha channel is the whole point, it is what lets an
// overlay be a HUD floating on the world instead of a rectangle in front of
// it.
constexpr UInt32 kFormatA8R8G8B8 = 21;

using SetRenderTargetFn = SInt32(__stdcall*)(void* self, UInt32 index, void* surface);
using GetRenderTargetFn = SInt32(__stdcall*)(void* self, UInt32 index, void** surface);
using SetRenderStateFn = SInt32(__stdcall*)(void* self, UInt32 state, UInt32 value);
using GetRenderStateFn = SInt32(__stdcall*)(void* self, UInt32 state, UInt32* value);
using CreateStateBlockFn = SInt32(__stdcall*)(void* self, UInt32 type, void** stateBlock);

// Capture and Apply share a signature: both take only the block itself.
using StateBlockMethodFn = SInt32(__stdcall*)(void* self);

// Present, for the hook that moves the submit to the end of the frame.
// Already declared above as kDevicePresent.

// D3DMATRIX (d3d9types.h): sixteen floats, row-major, m[row][column].
struct Matrix4 {
	float m[4][4];
};

static_assert(sizeof(Matrix4) == 16 * sizeof(float), "D3DMATRIX is sixteen floats");

using GetTransformFn = SInt32(__stdcall*)(void* self, UInt32 state, Matrix4* matrix);

// The setter side of the same pipeline: SetTransform (44), SetVertexDeclaration
// (87), SetFVF (89), SetVertexShader (92) and SetVertexShaderConstantF (94),
// counted from the same listing. Each sits one entry beside an already
// verified neighbour - its Get half, or the draw quartet - so a miscount
// here would have moved those too.
//
// Counters rather than interventions. The dual pass submits the same skinned
// bodies twice and one copy collapses onto a point, which is what a draw
// against missing bone matrices looks like; bone matrices travel through
// SetVertexShaderConstantF. So the question these settle: does the game run
// its vertex pipeline setup as often for one world render as for the other,
// or does one render draw against whatever the registers still hold.
constexpr UInt32 kDeviceSetTransform = 44;
constexpr UInt32 kDeviceSetVertexDeclaration = 87;
constexpr UInt32 kDeviceSetFVF = 89;
constexpr UInt32 kDeviceSetVertexShader = 92;
constexpr UInt32 kDeviceSetVertexShaderConstantF = 94;

using SetTransformFn = SInt32(__stdcall*)(void* self, UInt32 state, const Matrix4* matrix);
using SetVertexDeclarationFn = SInt32(__stdcall*)(void* self, void* declaration);
using SetFVFFn = SInt32(__stdcall*)(void* self, UInt32 fvf);
using SetVertexShaderFn = SInt32(__stdcall*)(void* self, void* shader);
using SetVertexShaderConstantFFn = SInt32(__stdcall*)(void* self, UInt32 startRegister,
                                                      const float* data, UInt32 vector4fCount);

// CreateVertexBuffer (26), framed by two verified neighbours: CreateTexture
// (23) three entries up and CreateRenderTarget (28) two entries down.
// Hooked so the buffers the game creates can have their Lock counted.
//
// IDirect3DVertexBuffer9 inherits IDirect3DResource9, so its own methods
// start at 11: Lock, then Unlock - the same counting that placed
// kSurfaceGetDesc. Gamebryo's own documentation says software-skinned
// geometry is repacked into video-memory vertex buffers as it renders;
// those repacks are locks, and counting them either side of each world
// render says whether the packing happens once per frame or once per pass.
constexpr UInt32 kDeviceCreateVertexBuffer = 26;
constexpr UInt32 kVertexBufferLock = 11;

// D3DLOCK_DISCARD (d3d9types.h, line 1696). A lock that throws the old
// contents away. Geometry drawn from a buffer that was discarded and never
// refilled is one candidate for what bodies collapsed onto a point are.
constexpr UInt32 kLockDiscard = 0x2000;

// IUnknown::Release, method two of the three every interface starts with.
// For letting go of the probe buffer that activates the Lock counter.
constexpr UInt32 kUnknownRelease = 2;

using CreateVertexBufferFn = SInt32(__stdcall*)(void* self, UInt32 length, UInt32 usage,
                                                UInt32 fvf, UInt32 pool, void** vertexBuffer,
                                                void** sharedHandle);
using VertexBufferLockFn = SInt32(__stdcall*)(void* self, UInt32 offset, UInt32 size,
                                              void** data, UInt32 flags);
using ReleaseFn = UInt32(__stdcall*)(void* self);

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
