#include "render/TabletQuad.h"

#include <vector>

#include "core/Log.h"
#include "game/ItemIcons.h"
#include "core/Rotation.h"
#include "platform/Win32Min.h"
#include "render/ControllerProjection.h"
#include "render/D3D11Types.h"
#include "render/D3D9Types.h"
#include "render/DdsImage.h"
#include "render/InterfaceRenderHook.h"
#include "vr/MirrorDust.h"
#include "vr/MirrorPage.h"
#include "vr/TabletPaper.h"

namespace obvr::render {
namespace {

// The texture coordinate has three parts: u and v, and q for the mirror's
// glass laid through its picture's camera (vr/MirrorGlassMap.h); 1 for every
// other piece, where the device ignores it.
struct TabletVertex {
	float x, y, z, rhw;
	UInt32 colour;
	float u, v, q;
};
static_assert(sizeof(TabletVertex) == 32, "XYZRHW|DIFFUSE|TEX1|TEXCOORDSIZE3(0) is a 32-byte vertex");
// D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE3(0).
constexpr UInt32 kFvfXyzRhwDiffuseTex1 = 0x004 | 0x040 | 0x100 | 0x10000;

// D3DTA_TEXTURE: the stage's texture as its argument (d3d9types.h).
constexpr UInt32 kTaTexture = 2;
// The laid glass's states (d3d9types.h): D3DTSS_TEXTURETRANSFORMFLAGS with
// D3DTTFF_COUNT3 | D3DTTFF_PROJECTED (u and v divided by q per pixel), and
// D3DTADDRESS_BORDER with D3DSAMP_BORDERCOLOR black past the texture's edge.
constexpr UInt32 kTssTextureTransformFlags = 24;
constexpr UInt32 kTtffDisable = 0;
constexpr UInt32 kTtffCount3Projected = 3 | 256;
constexpr UInt32 kTextureAddressBorder = 4;
constexpr UInt32 kSamplerBorderColor = 4;

// The eye's viewport: the device's, or the eye's half on a single-pass frame.
bool EyeViewport(void* device, const ControllerModels::WorldView& view, d3d9::Viewport& viewport) {
	auto getViewport = d3d9::Method<d3d9::GetViewportFn>(device, d3d9::kDeviceGetViewport);
	if (getViewport == nullptr || d3d11::Failed(getViewport(device, &viewport))) {
		return false;
	}
	if (view.hasEyeRect) {
		viewport.x = view.eyeRectX;
		viewport.y = view.eyeRectY;
		viewport.width = view.eyeRectWidth;
		viewport.height = view.eyeRectHeight;
	}
	return viewport.width != 0 && viewport.height != 0;
}

// A piece's four corners into the eye's pixels; false when one is behind the eye.
bool ProjectCorners(const TabletQuadPiece& piece, UInt32 colour, const ControllerModels::WorldView& view,
                    const d3d9::Matrix4& projection, const d3d9::Viewport& viewport, TabletVertex (&out)[4]) {
	const ControllerModels::WorldController& hand = view.hands[1];
	const NiMatrix33 toEye = InverseRotation(view.eyeRot);
	for (int i = 0; i < 4; ++i) {
		// ControllerVertexToGame's axes, the formula the mirror's camera shares.
		const NiPoint3 world = vr::GlassCornerInWorld(hand.pos, hand.rot, view.unitsPerMetre, piece.corners[i]);
		EyePixel pixel;
		if (!ProjectThroughD3D(toEye * (world - view.eyePos), projection.m, static_cast<float>(viewport.x),
		                       static_cast<float>(viewport.y), static_cast<float>(viewport.width),
		                       static_cast<float>(viewport.height), viewport.minZ, viewport.maxZ, pixel)) {
			return false;
		}
		out[i] = TabletVertex{pixel.x, pixel.y, pixel.depth, pixel.rhw, colour, piece.u[i], piece.v[i], 1.0f};
	}
	return true;
}

// The game's page paper as a texture on the device (vr/TabletPaper.h): the
// file through the engine's opener, decoded on the CPU, its colour weighed
// by its alpha (the pieces go over the eye premultiplied, as the layer does),
// into a managed texture in A8R8G8B8's order. Null when any step fails.
void* MakePaperTexture(void* device) {
	if (device == nullptr) {
		return nullptr;
	}
	static const long long ticksPerSecond = ReadPerformanceFrequency();
	const long long before = ReadPerformanceCounter();
	std::vector<UInt8> bytes;
	std::vector<Pixel> pixels;
	UInt32 width = 0;
	UInt32 height = 0;
	if (!game::ReadGameFile(vr::kTabletPaperFile, bytes) ||
	    !DecodeDds(bytes.data(), static_cast<UInt32>(bytes.size()), pixels, width, height) ||
	    width != static_cast<UInt32>(vr::kTabletPaperSide) || height != static_cast<UInt32>(vr::kTabletPaperSide)) {
		OBVR_LOG("Tablet: the paper %s %s - the tablet's own pieces keep their plain colour", vr::kTabletPaperFile,
		         bytes.empty() ? "did not open" : "is not a 1024 by 1024 DDS this reads");
		return nullptr;
	}
	auto create = d3d9::Method<d3d9::CreateTextureFn>(device, d3d9::kDeviceCreateTexture);
	void* texture = nullptr;
	if (create == nullptr || d3d11::Failed(create(device, width, height, 1, 0, d3d9::kFormatA8R8G8B8,
	                                              d3d9::kPoolManaged, &texture, nullptr)) ||
	    texture == nullptr) {
		OBVR_LOG("Tablet: no texture for the paper - the tablet's own pieces keep their plain colour");
		return nullptr;
	}
	auto lock = d3d9::Method<d3d9::TextureLockRectFn>(texture, d3d9::kTextureLevelLockRect);
	auto unlock = d3d9::Method<d3d9::TextureUnlockRectFn>(texture, d3d9::kTextureLevelUnlockRect);
	d3d9::LockedRect locked{};
	if (lock == nullptr || unlock == nullptr || d3d11::Failed(lock(texture, 0, &locked, nullptr, 0)) ||
	    locked.bits == nullptr) {
		d3d11::Release(texture);
		OBVR_LOG("Tablet: the paper's texture did not lock - the tablet's own pieces keep their plain colour");
		return nullptr;
	}
	for (UInt32 y = 0; y < height; ++y) {
		UInt8* row = static_cast<UInt8*>(locked.bits) + y * static_cast<UInt32>(locked.pitch);
		for (UInt32 x = 0; x < width; ++x) {
			const Pixel& p = pixels[y * width + x];
			row[4 * x + 0] = static_cast<UInt8>(p.b * p.a / 255);
			row[4 * x + 1] = static_cast<UInt8>(p.g * p.a / 255);
			row[4 * x + 2] = static_cast<UInt8>(p.r * p.a / 255);
			row[4 * x + 3] = p.a;
		}
	}
	unlock(texture, 0);
	const double ms = ticksPerSecond > 0 ? 1000.0 * static_cast<double>(ReadPerformanceCounter() - before) /
	                                           static_cast<double>(ticksPerSecond)
	                                     : 0.0;
	OBVR_LOG("Tablet: the game's paper %s on the tablet's own pieces (%u bytes, %ux%u), made in %.1f ms",
	         vr::kTabletPaperFile, static_cast<UInt32>(bytes.size()), width, height, ms);
	return texture;
}

// The mirror's dust as a texture on the device (vr/MirrorDust.h): made once on
// the CPU, colour already weighed by its alpha, into a managed texture in
// A8R8G8B8's order. Null when any step fails (the glass then has no dust).
void* MakeDustTexture(void* device) {
	if (device == nullptr) {
		return nullptr;
	}
	static const long long ticksPerSecond = ReadPerformanceFrequency();
	const long long before = ReadPerformanceCounter();
	constexpr UInt32 side = vr::kMirrorDustSide;
	auto create = d3d9::Method<d3d9::CreateTextureFn>(device, d3d9::kDeviceCreateTexture);
	void* texture = nullptr;
	if (create == nullptr || d3d11::Failed(create(device, side, side, 1, 0, d3d9::kFormatA8R8G8B8, d3d9::kPoolManaged,
	                                              &texture, nullptr)) ||
	    texture == nullptr) {
		OBVR_LOG("Tablet: no texture for the mirror's dust - the glass stays clean");
		return nullptr;
	}
	auto lock = d3d9::Method<d3d9::TextureLockRectFn>(texture, d3d9::kTextureLevelLockRect);
	auto unlock = d3d9::Method<d3d9::TextureUnlockRectFn>(texture, d3d9::kTextureLevelUnlockRect);
	d3d9::LockedRect locked{};
	if (lock == nullptr || unlock == nullptr || d3d11::Failed(lock(texture, 0, &locked, nullptr, 0)) ||
	    locked.bits == nullptr) {
		d3d11::Release(texture);
		OBVR_LOG("Tablet: the mirror's dust texture did not lock - the glass stays clean");
		return nullptr;
	}
	std::vector<UInt32> texels(side * side);
	vr::FillMirrorDust(texels.data(), side);
	for (UInt32 y = 0; y < side; ++y) {
		UInt8* row = static_cast<UInt8*>(locked.bits) + y * static_cast<UInt32>(locked.pitch);
		for (UInt32 x = 0; x < side; ++x) {
			const UInt32 t = texels[y * side + x];
			row[4 * x + 0] = static_cast<UInt8>(t & 0xFFu);
			row[4 * x + 1] = static_cast<UInt8>((t >> 8) & 0xFFu);
			row[4 * x + 2] = static_cast<UInt8>((t >> 16) & 0xFFu);
			row[4 * x + 3] = static_cast<UInt8>(t >> 24);
		}
	}
	unlock(texture, 0);
	const double ms = ticksPerSecond > 0 ? 1000.0 * static_cast<double>(ReadPerformanceCounter() - before) /
	                                           static_cast<double>(ticksPerSecond)
	                                     : 0.0;
	OBVR_LOG("Tablet: the mirror's dust made on the CPU (%ux%u), in %.2f ms", side, side, ms);
	return texture;
}

// What a piece is drawn with. GlassLaid: the reflection laid through its
// picture's camera, with projective coordinates.
enum class PieceKind : UInt8 { Plain, Layer, Paper, Dust, Glass, GlassLaid };

// The reflection under the glass: a real mirror gives back about 90 percent
// (docs/mirror-spec.md 7), opaque.
constexpr UInt32 kMirrorReflectionTint = 0xFFE6E6E6u;

// Where a piece's projected corners lie in the eye's pixels, and the eye's
// viewport and place: what the mirror's pass reads of this eye (MirrorPage).
vr::GlassOnScreen GlassSeenIn(const TabletVertex (&corners)[4], const d3d9::Viewport& viewport,
                              const NiPoint3& eye) {
	vr::GlassOnScreen s;
	s.drawn = true;
	s.minX = s.maxX = corners[0].x;
	s.minY = s.maxY = corners[0].y;
	for (const TabletVertex& c : corners) {
		s.minX = c.x < s.minX ? c.x : s.minX;
		s.maxX = c.x > s.maxX ? c.x : s.maxX;
		s.minY = c.y < s.minY ? c.y : s.minY;
		s.maxY = c.y > s.maxY ? c.y : s.maxY;
	}
	s.viewportX = static_cast<float>(viewport.x);
	s.viewportY = static_cast<float>(viewport.y);
	s.viewportWidth = static_cast<float>(viewport.width);
	s.viewportHeight = static_cast<float>(viewport.height);
	s.eye = eye;
	return s;
}

// The glass piece with the reflection. Without a picture's camera (MirrorSteady
// 0) the picture is stuck to its corners, flipped as a mirror shows it
// (vr::GlassTexCoords). With one, each corner as the glass stands now is
// projected through that camera (vr::MapGlassCorners) and the piece drawn
// projectively; a glass that cannot be mapped is drawn plain dark instead,
// never with a picture wrapped round. `laid` counts the draws with a camera:
// all, with a corner past the picture, not mapped.
void LayGlass(const TabletQuadPiece& piece, const MirrorGlassUv& uv, const ControllerModels::WorldView& view,
              TabletVertex (&v)[4], PieceKind& kind, void*& texture, UInt32 (&laid)[3]) {
	if (!uv.view.valid) {
		float u[4];
		float w[4];
		vr::GlassTexCoords(uv.rect, u, w);
		for (int corner = 0; corner < 4; ++corner) {
			v[corner].u = u[corner];
			v[corner].v = w[corner];
		}
		return;
	}
	const ControllerModels::WorldController& hand = view.hands[1];
	NiPoint3 world[4];
	for (int i = 0; i < 4; ++i) {
		world[i] = vr::GlassCornerInWorld(hand.pos, hand.rot, view.unitsPerMetre, piece.corners[i]);
	}
	float s[4];
	float t[4];
	float q[4];
	const vr::GlassMap map = vr::MapGlassCorners(uv.view, world, s, t, q);
	++laid[0];
	if (map != vr::GlassMap::Inside && map != vr::GlassMap::Outside) {
		++laid[2];
		kind = PieceKind::Plain;
		texture = nullptr;
		for (TabletVertex& corner : v) {
			corner.colour = piece.colour;
		}
		return;
	}
	laid[1] += map == vr::GlassMap::Outside ? 1u : 0u;
	kind = PieceKind::GlassLaid;
	for (int corner = 0; corner < 4; ++corner) {
		v[corner].u = s[corner];
		v[corner].v = t[corner];
		v[corner].q = q[corner];
	}
}

// The pieces over the eye's picture, the device's states captured before and
// put back after: a layer's piece takes the layer's colour and alpha, a
// paper's the paper's times its vertices', a plain one its vertices'; the
// dust and the glass's reflection their own texture's, as the layer's does.
// `textures[i]` is the texture of piece i (none for a plain one).
bool DrawPieces(void* device, void* const* textures, const PieceKind* kinds, const TabletVertex (*vertices)[4],
                UInt32 count) {
	auto createBlock = d3d9::Method<d3d9::CreateStateBlockFn>(device, d3d9::kDeviceCreateStateBlock);
	auto setState = d3d9::Method<d3d9::SetRenderStateFn>(device, d3d9::kDeviceSetRenderState);
	auto setStage = d3d9::Method<d3d9::SetTextureStageStateFn>(device, d3d9::kDeviceSetTextureStageState);
	auto setSampler = d3d9::Method<d3d9::SetSamplerStateFn>(device, d3d9::kDeviceSetSamplerState);
	auto setTexture = d3d9::Method<d3d9::SetTextureFn>(device, d3d9::kDeviceSetTexture);
	auto setFvf = d3d9::Method<d3d9::SetFVFFn>(device, d3d9::kDeviceSetFVF);
	auto setVertexShader = d3d9::Method<d3d9::SetVertexShaderFn>(device, d3d9::kDeviceSetVertexShader);
	auto setPixelShader = d3d9::Method<d3d9::SetPixelShaderFn>(device, d3d9::kDeviceSetPixelShader);
	auto beginScene = d3d9::Method<d3d9::SceneBracketFn>(device, d3d9::kDeviceBeginScene);
	auto endScene = d3d9::Method<d3d9::SceneBracketFn>(device, d3d9::kDeviceEndScene);
	auto draw = d3d9::Method<d3d9::DrawPrimitiveUPFn>(device, d3d9::kDeviceDrawPrimitiveUP);
	void* block = nullptr;
	if (createBlock == nullptr || setState == nullptr || setStage == nullptr || setSampler == nullptr ||
	    setTexture == nullptr || setFvf == nullptr || setVertexShader == nullptr || setPixelShader == nullptr ||
	    beginScene == nullptr || endScene == nullptr || draw == nullptr ||
	    d3d11::Failed(createBlock(device, d3d9::kStateBlockTypeAll, &block)) || block == nullptr) {
		return false;
	}

	// The layer's colour is already weighed by its alpha (HudLayer's capture:
	// one, then what remains), so it goes over the eye as premultiplied:
	// one, inverse source alpha. Tested against the world's depth, so what
	// stands in front - the tapping hand - hides it; never written into it.
	setVertexShader(device, nullptr);
	setPixelShader(device, nullptr);
	setTexture(device, 0, nullptr);
	setStage(device, 0, d3d9::kTssColorOp, d3d9::kTopSelectArg1);
	setStage(device, 0, d3d9::kTssColorArg1, kTaTexture);
	setStage(device, 0, d3d9::kTssAlphaOp, d3d9::kTopSelectArg1);
	setStage(device, 0, d3d9::kTssAlphaArg1, kTaTexture);
	setStage(device, 1, d3d9::kTssColorOp, d3d9::kTopDisable);
	setStage(device, 1, d3d9::kTssAlphaOp, d3d9::kTopDisable);
	setSampler(device, 0, d3d9::kSamplerAddressU, d3d9::kTextureAddressClamp);
	setSampler(device, 0, d3d9::kSamplerAddressV, d3d9::kTextureAddressClamp);
	setSampler(device, 0, kSamplerBorderColor, 0xFF000000u);
	setSampler(device, 0, d3d9::kSamplerMagFilter, d3d9::kTexFilterLinear);
	setSampler(device, 0, d3d9::kSamplerMinFilter, d3d9::kTexFilterLinear);
	setState(device, d3d9::kRenderStateZEnable, 1);
	setState(device, d3d9::kRenderStateZWriteEnable, 0);
	setState(device, d3d9::kRenderStateZFunc, d3d9::kCmpLessEqual);
	setState(device, d3d9::kRenderStateAlphaTestEnable, 0);
	setState(device, d3d9::kRenderStateAlphaBlendEnable, 1);
	setState(device, d3d9::kRenderStateSeparateAlphaBlendEnable, 0);
	setState(device, d3d9::kRenderStateSrcBlend, d3d9::kBlendOne);
	setState(device, d3d9::kRenderStateDestBlend, d3d9::kBlendInvSrcAlpha);
	setState(device, d3d9::kRenderStateFogEnable, 0);
	setState(device, d3d9::kRenderStateLighting, 0);
	setState(device, d3d9::kRenderStateCullMode, d3d9::kCullNone);
	setState(device, d3d9::kRenderStateStencilEnable, 0);
	setState(device, d3d9::kRenderStateScissorTestEnable, 0);
	setState(device, d3d9::kRenderStateClipping, 0);
	setState(device, d3d9::kRenderStateClipPlaneEnable, 0);
	setState(device, d3d9::kRenderStateColorWriteEnable, d3d9::kColorWriteAll);
	setFvf(device, kFvfXyzRhwDiffuseTex1);
	const bool openedScene = !d3d11::Failed(beginScene(device));
	bool drawn = count > 0;
	setStage(device, 0, d3d9::kTssColorArg2, d3d9::kTaDiffuse);
	setStage(device, 0, d3d9::kTssAlphaArg2, d3d9::kTaDiffuse);
	for (UInt32 i = 0; i < count; ++i) {
		const bool onPaper = kinds[i] == PieceKind::Paper;
		const bool laid = kinds[i] == PieceKind::GlassLaid;
		const bool glass = kinds[i] == PieceKind::Glass || laid;
		const UInt32 argument = kinds[i] == PieceKind::Plain ? d3d9::kTaDiffuse : kTaTexture;
		const UInt32 op = onPaper || glass ? d3d9::kTopModulate : d3d9::kTopSelectArg1;
		// The laid glass divides u and v by q per pixel, and reads black past
		// the texture's edge (the texture is black outside its picture too).
		setStage(device, 0, kTssTextureTransformFlags, laid ? kTtffCount3Projected : kTtffDisable);
		setSampler(device, 0, d3d9::kSamplerAddressU, laid ? kTextureAddressBorder : d3d9::kTextureAddressClamp);
		setSampler(device, 0, d3d9::kSamplerAddressV, laid ? kTextureAddressBorder : d3d9::kTextureAddressClamp);
		setTexture(device, 0, textures[i]);
		setStage(device, 0, d3d9::kTssColorOp, op);
		// The reflection: its colour under the glass's tint, opaque whatever
		// alpha the frame it was copied from held (the vertices' alpha).
		setStage(device, 0, d3d9::kTssAlphaOp, glass ? d3d9::kTopSelectArg2 : op);
		setStage(device, 0, d3d9::kTssColorArg1, argument);
		setStage(device, 0, d3d9::kTssAlphaArg1, argument);
		drawn = !d3d11::Failed(draw(device, d3d9::kPrimitiveTriangleStrip, 2, vertices[i], sizeof(TabletVertex))) &&
		        drawn;
	}
	if (openedScene) {
		endScene(device);
	}
	setTexture(device, 0, nullptr);
	auto apply = d3d9::Method<d3d9::StateBlockMethodFn>(block, d3d9::kStateBlockApply);
	if (apply != nullptr) {
		apply(block);
	}
	d3d11::Release(block);
	return drawn;
}

}  // namespace

void* TabletQuad::Paper(void* device) {
	if (!m_paperTried) {
		m_paperTried = true;
		m_paper = MakePaperTexture(device);
	}
	return m_paper;
}

void* TabletQuad::Dust(void* device) {
	if (!m_dustTried) {
		m_dustTried = true;
		m_dust = MakeDustTexture(device);
	}
	return m_dust;
}

bool TabletQuad::DrawIntoWorld(void* device, const ControllerModels::WorldView& view, bool leftEye) {
	if (!m_frame.valid || m_frame.texture == nullptr || device == nullptr || !view.eyeValid ||
	    !view.hands[1].valid) {
		return false;
	}
	// The projection the eye's picture was drawn with, as the controllers
	// take it - or, when the last perspective one set is no longer the world
	// camera's, the one this frame's first eye was drawn with. In menus the
	// 2D pass between the eyes' captures draws the player's figure through a
	// perspective of its own (seen 2026-11-17, single pass: the right eye
	// refused); a single pass draws both eyes through the one projection.
	d3d9::Matrix4 projection{};
	d3d9::Viewport viewport{};
	TabletVertex vertices[kTabletPieces][4];
	PieceKind kinds[kTabletPieces] = {};
	void* textures[kTabletPieces] = {};
	void* const paper = Paper(device);
	void* const dust = Dust(device);
	const int picture = vr::GlassPictureFor(leftEye, m_glassTexture[1] != nullptr);
	void* const glass = m_glassTexture[picture];
	UInt32 count = 0;
	const bool fresh = LastPerspectiveProjection(projection.m) && PerspectiveMatchesCamera(projection.m, view.tanHalfWidth);
	if (fresh) {
		m_projection = projection;
		m_projectionSerial = m_serial;
	} else if (m_projectionSerial == m_serial) {
		projection = m_projection;
	}
	bool projected = (fresh || m_projectionSerial == m_serial) && EyeViewport(device, view, viewport);
	vr::GlassOnScreen glassSeen;
	for (UInt32 i = 0; projected && i < kTabletPieces; ++i) {
		const TabletQuadPiece& piece = m_frame.pieces[i];
		if (!piece.valid) {
			continue;
		}
		if (piece.dust && dust == nullptr) {
			continue;  // no dust texture: the glass stays clean
		}
		const bool onPaper = piece.paper && paper != nullptr;
		const bool reflects = piece.glass && glass != nullptr;
		kinds[count] = piece.dust ? PieceKind::Dust
		               : reflects ? PieceKind::Glass
		               : onPaper  ? PieceKind::Paper
		               : piece.textured && !piece.paper ? PieceKind::Layer
		                                                : PieceKind::Plain;
		textures[count] = piece.dust ? dust : reflects ? glass : onPaper ? paper : m_frame.texture;
		const UInt32 colour = reflects ? kMirrorReflectionTint : onPaper ? piece.tint : piece.colour;
		projected = ProjectCorners(piece, colour, view, projection, viewport, vertices[count]);
		if (projected && piece.glass) {
			glassSeen = GlassSeenIn(vertices[count], viewport, view.eyePos);
		}
		if (projected && reflects) {
			LayGlass(piece, m_glassUv[picture], view, vertices[count], kinds[count], textures[count], m_glassLaid);
		}
		++count;
	}
	if (!projected || count == 0) {
		if (!m_refusedReported) {
			m_refusedReported = true;
			OBVR_LOG("Tablet: not drawn into the %s eye (%s) - the overlay shows it", leftEye ? "left" : "right",
			         !fresh && m_projectionSerial != m_serial ? "no world projection on the device this frame"
			                                                  : "no viewport, or a corner behind the eye");
		}
		return false;
	}
	if (!DrawPieces(device, textures, kinds, vertices, count)) {
		return false;
	}
	m_eyes |= leftEye ? 1u : 2u;
	if (glassSeen.drawn) {
		m_glassSeen[leftEye ? 0 : 1] = glassSeen;
		m_glassHandPos = view.hands[1].pos;
		m_glassHandRot = view.hands[1].rot;
		m_glassUnitsPerMetre = view.unitsPerMetre;
	}
	if (!m_reported) {
		m_reported = true;
		OBVR_LOG("Tablet: drawn into the %s eye with the game's projection - %u piece(s), the first's corners at %.0f,%.0f %.0f,%.0f "
		         "%.0f,%.0f %.0f,%.0f, viewport %ux%u",
		         leftEye ? "left" : "right", count, static_cast<double>(vertices[0][0].x),
		         static_cast<double>(vertices[0][0].y), static_cast<double>(vertices[0][1].x),
		         static_cast<double>(vertices[0][1].y), static_cast<double>(vertices[0][2].x),
		         static_cast<double>(vertices[0][2].y), static_cast<double>(vertices[0][3].x),
		         static_cast<double>(vertices[0][3].y), viewport.width, viewport.height);
	}
	return true;
}

}  // namespace obvr::render
