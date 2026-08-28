#include "render/EyeMirror.h"

#include "core/Log.h"
#include "render/D3D11Types.h"
#include "core/MathFns.h"
#include "game/GameCamera.h"
#include "render/D3D9Types.h"
#include "render/GameProjection.h"
#include "render/MenuShade.h"

namespace obvr::render {
namespace {

// Opaque black. The margin around Oblivion's frame, which the game never
// draws into and which would otherwise show whatever the memory held when the
// texture was made - in a headset, a border of noise around the world.
constexpr UInt32 kOpaqueBlack = 0xFF000000;

float Abs(float value) { return value < 0.0f ? -value : value; }

// Reads the back buffer's size and format, so the copy is made from something
// whose shape is known rather than assumed.
//
// Asked rather than assumed. Oblivion's back buffer could be X8R8G8B8 or
// A8R8G8B8 depending on how the device was created, and StretchRect between
// differing formats is a conversion the runtime may refuse - a refusal that
// arrives as one failed call per frame and no picture, with nothing saying
// which of the two formats was wrong.
bool DescribeBackBuffer(void* gameDevice, d3d9::SurfaceDesc& out) {
	auto getBackBuffer =
		d3d9::Method<d3d9::GetBackBufferFn>(gameDevice, d3d9::kDeviceGetBackBuffer);
	if (getBackBuffer == nullptr) {
		return false;
	}

	void* surface = nullptr;
	if (d3d11::Failed(getBackBuffer(gameDevice, 0, 0, d3d9::kBackBufferTypeMono, &surface)) ||
	    surface == nullptr) {
		return false;
	}

	auto getDesc = d3d9::Method<d3d9::GetDescFn>(surface, d3d9::kSurfaceGetDesc);
	const bool ok = getDesc != nullptr && !d3d11::Failed(getDesc(surface, &out));

	d3d11::Release(surface);
	return ok;
}

// Turns a fraction of an image into a pixel edge.
//
// Rounded rather than truncated, and clamped to the image, because these
// become rectangles handed to Direct3D: an edge one pixel outside its surface
// is not a picture that overhangs, it is an invalid call.
SInt32 EdgeOf(float fraction, UInt32 extent) {
	const float scaled = fraction * static_cast<float>(extent);
	SInt32 pixel = static_cast<SInt32>(scaled + 0.5f);
	if (pixel < 0) {
		pixel = 0;
	}
	if (pixel > static_cast<SInt32>(extent)) {
		pixel = static_cast<SInt32>(extent);
	}
	return pixel;
}

}  // namespace

bool EyeMirror::CreateOne(void* gameDevice, int index) {
	Eye& eye = m_eye[index];

	auto createTexture =
		d3d9::Method<d3d9::CreateTextureFn>(gameDevice, d3d9::kDeviceCreateTexture);
	if (createTexture == nullptr) {
		return false;
	}

	// One mip level. A render target with generated mips would be more work
	// per frame for detail no headset ever samples.
	if (d3d11::Failed(createTexture(gameDevice, m_width, m_height, 1,
	                                d3d9::kUsageRenderTarget, m_format, d3d9::kPoolDefault,
	                                &eye.texture, nullptr)) ||
	    eye.texture == nullptr) {
		eye.texture = nullptr;
		return false;
	}

	// The surface is what StretchRect writes into; the texture is what the
	// interop interface is queried from. Both refer to the same image, and
	// both references are held for the life of the mirror because looking
	// either up costs a call per frame for an answer that cannot change.
	auto getSurfaceLevel =
		d3d9::Method<d3d9::GetSurfaceLevelFn>(eye.texture, d3d9::kTextureGetSurfaceLevel);
	if (getSurfaceLevel == nullptr ||
	    d3d11::Failed(getSurfaceLevel(eye.texture, 0, &eye.surface)) ||
	    eye.surface == nullptr) {
		eye.surface = nullptr;
		return false;
	}

	auto* unknown = static_cast<d3d11::Unknown*>(eye.texture);
	if (unknown->vtbl == nullptr || unknown->vtbl->QueryInterface == nullptr ||
	    d3d11::Failed(unknown->vtbl->QueryInterface(unknown, &kIID_D3D9VkInteropTexture,
	                                                &eye.interop)) ||
	    eye.interop == nullptr) {
		eye.interop = nullptr;
		return false;
	}

	return ReadImageInfo(eye.interop, eye.image);
}

bool EyeMirror::Create(void* gameDevice, UInt32 textureWidth, UInt32 textureHeight,
                       const EyeProjection& leftEye, const EyeProjection& rightEye,
                       float gameFovDegrees, bool gameFovIsFor4x3, float cameraTanHalfWidth,
                       float cameraTanHalfHeight, float menuScale, float menuAspect) {
	Destroy();

	if (gameDevice == nullptr || textureWidth == 0 || textureHeight == 0) {
		return false;
	}

	d3d9::SurfaceDesc desc{};
	if (!DescribeBackBuffer(gameDevice, desc)) {
		OBVR_LOG("Mirror: the back buffer could not be described, so no eye copies were made");
		return false;
	}

	if (desc.width == 0 || desc.height == 0) {
		OBVR_LOG("Mirror: the back buffer reports a size of %ux%u", desc.width, desc.height);
		return false;
	}

	m_width = textureWidth;
	m_height = textureHeight;
	m_frameWidth = desc.width;
	m_frameHeight = desc.height;
	m_format = desc.format;

	for (int index = 0; index < 2; ++index) {
		if (!CreateOne(gameDevice, index)) {
			OBVR_LOG("Mirror: the %s eye's copy could not be made",
			         index == 0 ? "left" : "right");
			Destroy();
			return false;
		}
	}

	// The one line that decides whether any of this works, written out
	// whether it passes or fails.
	//
	// The claim being tested is that a D3D9 texture with D3DUSAGE_RENDERTARGET
	// comes out of DXVK carrying both TRANSFER_SRC and SAMPLED, where a
	// surface from CreateRenderTarget would carry only the first. That was
	// read second hand, out of DeepWiki rather than out of the DXVK source,
	// and this line is what makes it first hand on the machine it matters on.
	const BackBufferImage& left = m_eye[0].image;
	OBVR_LOG("Mirror: %ux%u textures, D3DFORMAT %u, Vulkan format %u, usage 0x%08X - %s",
	         m_width, m_height, m_format, left.format, left.usage,
	         IsSubmittableImage(left) ? "submittable"
	                                  : "NOT submittable, so alternate eyes cannot work");

	if (!IsSubmittableImage(left) || !IsSubmittableImage(m_eye[1].image)) {
		OBVR_LOG("Mirror: SteamVR requires TRANSFER_SRC (0x1) and SAMPLED (0x4) on a "
		         "submitted image, and one of them is missing");
		Destroy();
		return false;
	}

	// Three sources for the same question, in order of how close each sits to
	// what is actually drawn.
	//
	// 1. Oblivion's own NiCamera frustum, read at the start of this frame.
	//    This is what Gamebryo composes its matrices from, so it is the
	//    frustum rather than a consequence of one.
	// 2. The Direct3D projection matrix. A consequence, and on this game a
	//    misleading one - see below.
	// 3. The configured field of view, which is a person typing a number.
	//
	// The order was learned the hard way and the numbers are worth keeping.
	// The device's projection matrix reports 75.0 degrees across and 46.7
	// down, which is fDefaultFOV read as a horizontal field of view at 16:9.
	// The camera reports 91.3 and 59.8. And 1.0231 is tan(37.5) x 0.75 x 16/9
	// to seven figures - fDefaultFOV read as a 4:3 figure with the vertical
	// held and the horizontal widened, which is what the Widescreen Gaming
	// Forum's "perfect implementation of widescreen" means and what this
	// project guessed wrong about twice.
	//
	// So the matrix is real, well-formed, passes every plausibility check, and
	// describes a view the game is not drawing. Believing it made the world
	// 1.333 times too small - exactly 4/3 - which is also, in hindsight, why
	// leaning needed a HeadMovementScale of 3 to feel like anything.
	float tanHalfWidth = 0.0f;
	float tanHalfHeight = 0.0f;
	const char* source = nullptr;

	if (cameraTanHalfWidth > 0.0f && cameraTanHalfHeight > 0.0f) {
		tanHalfWidth = cameraTanHalfWidth;
		tanHalfHeight = cameraTanHalfHeight;
		source = "Oblivion's own camera frustum";
	} else {
		GameProjection projection;
		if (ReadGameProjection(gameDevice, projection)) {
			tanHalfWidth = projection.tanHalfWidth;
			tanHalfHeight = projection.tanHalfHeight;
			source = "the Direct3D projection matrix, which may not be the view being drawn";
		} else {
			const float aspect =
				static_cast<float>(m_frameWidth) / static_cast<float>(m_frameHeight);
			if (gameFovIsFor4x3) {
				tanHalfHeight =
					math::Tan(gameFovDegrees * 0.5f * math::kDegreesToRadians) * 0.75f;
				tanHalfWidth = tanHalfHeight * aspect;
			} else {
				tanHalfWidth = math::Tan(gameFovDegrees * 0.5f * math::kDegreesToRadians);
				tanHalfHeight = tanHalfWidth / aspect;
			}
			source = "the configured GameFovDegrees, because neither could be read";
		}
	}

	OBVR_LOG("Mirror: rendering frustum %.1f degrees across, %.1f down, from %s",
	         static_cast<double>(2.0f * math::Atan(tanHalfWidth) * math::kRadiansToDegrees),
	         static_cast<double>(2.0f * math::Atan(tanHalfHeight) * math::kRadiansToDegrees),
	         source);

	// Where the game's frame goes inside each eye's view. This is what makes
	// the world its real size rather than whatever magnification two
	// the world its real size rather than whatever magnification two
	// unrelated frustums happen to imply.
	for (int index = 0; index < 2; ++index) {
		const EyeProjection& eye_projection = index == 0 ? leftEye : rightEye;
		const PicturePlacement placement =
			PlacePictureFromTangents(eye_projection, tanHalfWidth, tanHalfHeight);

		Eye& eye = m_eye[index];
		eye.destination.left = EdgeOf(placement.uMin, m_width);
		eye.destination.right = EdgeOf(placement.uMax, m_width);
		eye.destination.top = EdgeOf(placement.vMin, m_height);
		eye.destination.bottom = EdgeOf(placement.vMax, m_height);

		eye.source.left = EdgeOf(placement.sourceUMin, m_frameWidth);
		eye.source.right = EdgeOf(placement.sourceUMax, m_frameWidth);
		eye.source.top = EdgeOf(placement.sourceVMin, m_frameHeight);
		eye.source.bottom = EdgeOf(placement.sourceVMax, m_frameHeight);



		if (placement.cropped) {
			m_cropped = true;
		}

		// A rectangle of no area is not a small picture, it is a failed call.
		// It would mean the arithmetic above produced nonsense, and one line
		// here is cheaper than finding that out from a black headset.
		if (eye.destination.right <= eye.destination.left ||
		    eye.destination.bottom <= eye.destination.top ||
		    eye.source.right <= eye.source.left || eye.source.bottom <= eye.source.top) {
			OBVR_LOG("Mirror: the %s eye's picture came out with no area, so the placement "
			         "arithmetic is wrong",
			         index == 0 ? "left" : "right");
			Destroy();
			return false;
		}
	}

	// The window both eyes show, per eye, for the held pair's single border.
	// From the source slices, because those are what differ: each eye crops
	// the frame at the opposite edge, and the strip only one eye shows is the
	// edge the other eye cannot fuse.
	m_eye[0].commonDestination =
		CommonWindowInEye(m_eye[0].source, m_eye[0].destination, m_eye[1].source);
	m_eye[1].commonDestination =
		CommonWindowInEye(m_eye[1].source, m_eye[1].destination, m_eye[0].source);

	// The flat placement, sized identically in both eyes.
	//
	// Derived from the two world placements rather than from each eye's own,
	// because those differ: the eyes are cropped at opposite edges, so their
	// rectangles come out 17 pixels apart across and 8 down on this headset.
	// For the world that is right - each eye is looking somewhere slightly
	// different. For one flat picture shared between them it is not: an image
	// shown at two different sizes cannot be fused, and what the eyes make of
	// it is a doubled edge on everything. Reported as nauseating, and fairly.
	//
	// The smaller of the two, so neither is asked for pixels it does not have.
	// Centred on each eye's own optical axis, which is what puts a flat picture
	// at infinity - the same place a cinema screen is, and the reason it reads
	// as one.
	SInt32 flatWidth = m_eye[0].destination.right - m_eye[0].destination.left;
	const SInt32 otherWidth = m_eye[1].destination.right - m_eye[1].destination.left;
	if (otherWidth < flatWidth) {
		flatWidth = otherWidth;
	}

	flatWidth = static_cast<SInt32>(static_cast<float>(flatWidth) * menuScale);

	// The height comes from the frame's own pixel shape, never from the
	// world's angular rectangle - and that distinction was learned from a
	// report, not foreseen. A flat picture is 2D content: menus and videos
	// are laid out in the frame's pixels, 2560x1440, and showing them
	// undistorted means showing them at that shape. The world's rectangle is
	// something else entirely - the angles the frame covers - and since
	// MatchHeadsetFov made those angles nearly square, deriving the flat
	// height from it squeezed every menu into a square. The main menu escaped
	// because it is placed before the camera frustum arrives, from a 16:9-ish
	// fallback - which is exactly how the difference was noticed: the ESC and
	// inventory menus looked wrong and the main menu did not, on the same
	// settings.
	//
	// menuAspect overrides the shape when set; the source crop below then
	// takes a matching slice so the result is a crop rather than a squeeze.
	const float frameAspect = m_frameHeight > 0
	                              ? static_cast<float>(m_frameWidth) /
	                                    static_cast<float>(m_frameHeight)
	                              : 1.7778f;
	const float displayAspect = menuAspect > 0.1f ? menuAspect : frameAspect;
	SInt32 flatHeight =
		displayAspect > 0.1f
			? static_cast<SInt32>(static_cast<float>(flatWidth) / displayAspect)
			: flatWidth;

	for (int index = 0; index < 2; ++index) {
		Eye& eye = m_eye[index];
		const EyeProjection& projection = index == 0 ? leftEye : rightEye;

		// The optical axis, not the middle of the world rectangle.
		//
		// This is what puts a flat picture at infinity, and getting it from the
		// rectangle instead was the doubling. The world rectangle is cropped,
		// and cropped at opposite edges in the two eyes, so its middle is not
		// where either eye is looking: measured here, the axes are 566 pixels
		// apart and the rectangle centres only 222, which places the image at
		// some arbitrary depth the eyes then fight over.
		//
		// Same offset from each eye's own axis means the same direction from
		// both eyes, which is what infinity is - and where a cinema screen
		// sits, which is why it reads as one.
		const float eyeWidth = projection.right - projection.left;
		const float eyeHeight = projection.bottom - projection.top;
		const SInt32 axisX =
			eyeWidth > 0.0f
				? static_cast<SInt32>(-projection.left / eyeWidth * static_cast<float>(m_width))
				: static_cast<SInt32>(m_width / 2);
		const SInt32 axisY =
			eyeHeight > 0.0f
				? static_cast<SInt32>(projection.bottom / eyeHeight *
			                          static_cast<float>(m_height))
				: static_cast<SInt32>(m_height / 2);

		eye.flatDestination.left = axisX - flatWidth / 2;
		eye.flatDestination.right = eye.flatDestination.left + flatWidth;
		eye.flatDestination.top = axisY - flatHeight / 2;
		eye.flatDestination.bottom = eye.flatDestination.top + flatHeight;
	}


	// The slice of the frame a flat picture takes, matching the shape asked
	// for so the result is a crop rather than a squeeze.
	m_flatSource.left = 0;
	m_flatSource.right = static_cast<SInt32>(m_frameWidth);
	m_flatSource.top = 0;
	m_flatSource.bottom = static_cast<SInt32>(m_frameHeight);

	if (menuAspect > 0.1f) {
		const SInt32 wanted =
			static_cast<SInt32>(static_cast<float>(m_frameWidth) / menuAspect);
		if (wanted > 0 && wanted < static_cast<SInt32>(m_frameHeight)) {
			const SInt32 margin = (static_cast<SInt32>(m_frameHeight) - wanted) / 2;
			m_flatSource.top = margin;
			m_flatSource.bottom = margin + wanted;
		}
	}

	OBVR_LOG("Mirror: the game's %ux%u frame at %.1f degrees sits at left x=%d..%d y=%d..%d, "
	         "right x=%d..%d y=%d..%d%s",
	         m_frameWidth, m_frameHeight,
	         static_cast<double>(2.0f * math::Atan(tanHalfWidth) * math::kRadiansToDegrees),
	         m_eye[0].destination.left, m_eye[0].destination.right, m_eye[0].destination.top,
	         m_eye[0].destination.bottom, m_eye[1].destination.left,
	         m_eye[1].destination.right, m_eye[1].destination.top, m_eye[1].destination.bottom,
	         m_cropped ? " - part of the frame did not fit and was cut" : "");

	// How much of the headset's view the picture actually fills, said as a
	// percentage because that is the number a person can check against what
	// they see. A small figure is not a fault: a 16:9 frame at an ordinary
	// field of view simply does not fill a headset, and the remainder is
	// black by design rather than by accident.
	const int fillW = static_cast<int>(
		100 * (m_eye[0].destination.right - m_eye[0].destination.left) /
		static_cast<SInt32>(m_width));
	const int fillH = static_cast<int>(
		100 * (m_eye[0].destination.bottom - m_eye[0].destination.top) /
		static_cast<SInt32>(m_height));
	OBVR_LOG("Mirror: the picture fills %d%% of the view across and %d%% down; the rest is "
	         "black. Raise Render.GameFovOverride to fill more, at the cost of pushing the "
	         "HUD further out",
	         fillW, fillH);

	// Black once, now, rather than every frame. Nothing draws into the margin
	// afterwards, so it stays as it is left here - and a per-frame fill would
	// be a full-screen write for a picture that never changes.
	auto colorFill = d3d9::Method<d3d9::ColorFillFn>(gameDevice, d3d9::kDeviceColorFill);
	if (colorFill == nullptr) {
		OBVR_LOG("Mirror: ColorFill is not reachable, so the margin keeps whatever the "
		         "memory held");
	} else {
		for (Eye& eye : m_eye) {
			if (d3d11::Failed(colorFill(gameDevice, eye.surface, nullptr, kOpaqueBlack))) {
				OBVR_LOG("Mirror: the margin could not be blacked out, so it shows whatever "
				         "the memory held");
				break;
			}
		}
	}

	// Linear, because the copy scales now - the frame and its place in the
	// texture are different sizes, and D3DTEXF_NONE is documented as being
	// for the case where they are not. Which filters StretchRect accepts is a
	// device capability rather than a certainty, so the answer is taken from
	// the device on the first copy instead of assumed here.
	m_filter = d3d9::kTexFilterLinear;
	return true;
}

bool EyeMirror::CopyBackBuffer(void* gameDevice, bool isLeft, bool bothEyes) {
	if (!IsReady() || gameDevice == nullptr) {
		return false;
	}

	auto stretchRect = d3d9::Method<d3d9::StretchRectFn>(gameDevice, d3d9::kDeviceStretchRect);
	auto getBackBuffer =
		d3d9::Method<d3d9::GetBackBufferFn>(gameDevice, d3d9::kDeviceGetBackBuffer);
	if (stretchRect == nullptr || getBackBuffer == nullptr) {
		return false;
	}

	void* source = nullptr;
	if (d3d11::Failed(getBackBuffer(gameDevice, 0, 0, d3d9::kBackBufferTypeMono, &source)) ||
	    source == nullptr) {
		return false;
	}

	// The margin has to be blacked out again whenever the placement changes
	// size, or the larger one leaves pixels standing around the smaller. That
	// is a ring of stale world around a menu, which looks like a fault rather
	// than a frame.
	//
	// Before deciding which eyes to fill, not after: blacking out both and
	// then filling only one leaves the other dark for a frame, which is one
	// eye going out every time a menu opens or closes. That was reported as
	// menus appearing to render twice, and it is the same thing seen from
	// outside.
	if (!m_everCopied || m_lastWasFlat != bothEyes) {
		auto colorFill = d3d9::Method<d3d9::ColorFillFn>(gameDevice, d3d9::kDeviceColorFill);
		if (colorFill != nullptr) {
			for (Eye& eye : m_eye) {
				colorFill(gameDevice, eye.surface, nullptr, kOpaqueBlack);
			}
		}
		m_lastWasFlat = bothEyes;
		m_everCopied = true;
		m_primed = false;
	}

	// Both on the first pass, and after any blacking out. After that only the
	// eye this frame belongs to - the other one keeps the picture from its own
	// last turn, which is what makes the two eyes differ at all.
	const bool everyEye = bothEyes || !m_primed;
	const int first = everyEye ? 0 : (isLeft ? 0 : 1);
	const int last = everyEye ? 1 : first;

	bool ok = true;
	for (int index = first; index <= last; ++index) {
		const Eye& eye = m_eye[index];
		const d3d9::Rect& destination = bothEyes ? eye.flatDestination : eye.destination;
		const d3d9::Rect& sourceRect = bothEyes ? m_flatSource : eye.source;
		SInt32 result =
			stretchRect(gameDevice, source, &sourceRect, eye.surface, &destination, m_filter);

		// A device may refuse linear stretching - it is a capability, not a
		// guarantee. Point filtering is worse to look at and better than no
		// picture, and the fallback is remembered so the refusal costs one
		// extra call rather than one per frame for ever.
		if (d3d11::Failed(result) && m_filter != d3d9::kTexFilterPoint) {
			OBVR_LOG("Mirror: linear stretching was refused, falling back to point");
			m_filter = d3d9::kTexFilterPoint;
			result = stretchRect(gameDevice, source, &sourceRect, eye.surface, &destination,
			                     m_filter);
		}

		if (d3d11::Failed(result)) {
			ok = false;
		}
	}

	d3d11::Release(source);

	if (ok) {
		m_primed = true;

		// The copy replaced the pixels any menu dressing was painted on, so
		// the pair is bare again. This is also the whole exit path: the first
		// world frame after a menu closes undresses the pair by existing.
		m_heldShaded = false;
	}
	return ok;
}

bool EyeMirror::PrepareHeldShade(void* gameDevice, UInt32 shadeColorArgb,
                                 bool trimToSharedWindow) {
	if (m_heldShaded) {
		return true;
	}
	if (!IsReady() || gameDevice == nullptr) {
		return false;
	}

	// Set first, not on success. A device that refuses part of this should
	// refuse it once per episode, not once per frame - and the pictures are
	// replaced wholesale by the next copy either way.
	m_heldShaded = true;

	// The strips first, with ColorFill, which needs no pipeline state at all.
	if (trimToSharedWindow) {
		auto colorFill = d3d9::Method<d3d9::ColorFillFn>(gameDevice, d3d9::kDeviceColorFill);
		if (colorFill != nullptr) {
			for (Eye& eye : m_eye) {
				d3d9::Rect strips[4];
				const int count = EdgeStrips(eye.destination, eye.commonDestination, strips);
				for (int index = 0; index < count; ++index) {
					colorFill(gameDevice, eye.surface, &strips[index], kOpaqueBlack);
				}
			}
		}
	}

	if (shadeColorArgb == 0) {
		return true;
	}

	// The tint: one alpha-blended quad per eye, over the world rectangle. A
	// blend cannot come from ColorFill, so this is a draw, and a draw on the
	// game's device is a guest - everything it touches is captured first and
	// put back after, and the render target, which no state block carries, is
	// saved by hand.
	auto getTarget = d3d9::Method<d3d9::GetRenderTargetFn>(gameDevice, d3d9::kDeviceGetRenderTarget);
	auto setTarget = d3d9::Method<d3d9::SetRenderTargetFn>(gameDevice, d3d9::kDeviceSetRenderTarget);
	auto getDepth = d3d9::Method<d3d9::GetDepthStencilSurfaceFn>(
		gameDevice, d3d9::kDeviceGetDepthStencilSurface);
	auto setDepth = d3d9::Method<d3d9::SetDepthStencilSurfaceFn>(
		gameDevice, d3d9::kDeviceSetDepthStencilSurface);
	auto createBlock = d3d9::Method<d3d9::CreateStateBlockFn>(
		gameDevice, d3d9::kDeviceCreateStateBlock);
	auto setState = d3d9::Method<d3d9::SetRenderStateFn>(gameDevice, d3d9::kDeviceSetRenderState);
	auto setTexture = d3d9::Method<d3d9::SetTextureFn>(gameDevice, d3d9::kDeviceSetTexture);
	auto setStage = d3d9::Method<d3d9::SetTextureStageStateFn>(
		gameDevice, d3d9::kDeviceSetTextureStageState);
	auto setFvf = d3d9::Method<d3d9::SetFVFFn>(gameDevice, d3d9::kDeviceSetFVF);
	auto setVertexShader = d3d9::Method<d3d9::SetVertexShaderFn>(
		gameDevice, d3d9::kDeviceSetVertexShader);
	auto setPixelShader = d3d9::Method<d3d9::SetPixelShaderFn>(
		gameDevice, d3d9::kDeviceSetPixelShader);
	auto beginScene = d3d9::Method<d3d9::SceneBracketFn>(gameDevice, d3d9::kDeviceBeginScene);
	auto endScene = d3d9::Method<d3d9::SceneBracketFn>(gameDevice, d3d9::kDeviceEndScene);
	auto draw = d3d9::Method<d3d9::DrawPrimitiveUPFn>(gameDevice, d3d9::kDeviceDrawPrimitiveUP);

	if (getTarget == nullptr || setTarget == nullptr || getDepth == nullptr ||
	    setDepth == nullptr || createBlock == nullptr || setState == nullptr ||
	    setTexture == nullptr || setStage == nullptr || setFvf == nullptr ||
	    setVertexShader == nullptr || setPixelShader == nullptr || beginScene == nullptr ||
	    endScene == nullptr || draw == nullptr) {
		if (!m_shadeFailureLogged) {
			m_shadeFailureLogged = true;
			OBVR_LOG("Mirror: the menu shade could not reach the device, so the held pair "
			         "stays unshaded");
		}
		return false;
	}

	void* previousTarget = nullptr;
	void* previousDepth = nullptr;
	getTarget(gameDevice, 0, &previousTarget);
	getDepth(gameDevice, &previousDepth);

	void* block = nullptr;
	if (d3d11::Failed(createBlock(gameDevice, d3d9::kStateBlockTypeAll, &block)) ||
	    block == nullptr) {
		d3d11::Release(previousTarget);
		d3d11::Release(previousDepth);
		if (!m_shadeFailureLogged) {
			m_shadeFailureLogged = true;
			OBVR_LOG("Mirror: no state block for the menu shade, so the held pair stays "
			         "unshaded");
		}
		return false;
	}

	// The quad's pipeline: no shaders, no texture, the vertex colour selected
	// straight through both stages, an ordinary over-blend, and every test
	// that could silently reject a fixed-function draw switched off. The
	// coordinates are pre-transformed texture pixels, so no transform is
	// involved at all.
	setPixelShader(gameDevice, nullptr);
	setVertexShader(gameDevice, nullptr);
	setTexture(gameDevice, 0, nullptr);
	setStage(gameDevice, 0, d3d9::kStageColorOp, d3d9::kTextureOpSelectArg1);
	setStage(gameDevice, 0, d3d9::kStageColorArg1, d3d9::kTextureArgDiffuse);
	setStage(gameDevice, 0, d3d9::kStageAlphaOp, d3d9::kTextureOpSelectArg1);
	setStage(gameDevice, 0, d3d9::kStageAlphaArg1, d3d9::kTextureArgDiffuse);
	setState(gameDevice, d3d9::kRenderStateZEnable, 0);
	setState(gameDevice, d3d9::kRenderStateZWriteEnable, 0);
	setState(gameDevice, d3d9::kRenderStateAlphaTestEnable, 0);
	setState(gameDevice, d3d9::kRenderStateAlphaBlendEnable, 1);
	setState(gameDevice, d3d9::kRenderStateSeparateAlphaBlendEnable, 0);
	setState(gameDevice, d3d9::kRenderStateSrcBlend, d3d9::kBlendSrcAlpha);
	setState(gameDevice, d3d9::kRenderStateDestBlend, d3d9::kBlendInvSrcAlpha);
	setState(gameDevice, d3d9::kRenderStateFogEnable, 0);
	setState(gameDevice, d3d9::kRenderStateLighting, 0);
	setState(gameDevice, d3d9::kRenderStateCullMode, d3d9::kCullNone);
	setState(gameDevice, d3d9::kRenderStateStencilEnable, 0);
	setState(gameDevice, d3d9::kRenderStateScissorTestEnable, 0);
	setState(gameDevice, d3d9::kRenderStateClipping, 0);
	setState(gameDevice, d3d9::kRenderStateClipPlaneEnable, 0);
	setState(gameDevice, d3d9::kRenderStateColorWriteEnable, d3d9::kColorWriteAll);
	setFvf(gameDevice, d3d9::kFvfXyzRhwDiffuse);

	// This runs from the Present hook, where the game's own scene bracket is
	// closed, so the draw brings its own. If the bracket were already open,
	// BeginScene fails and the draw runs inside the open one - either way the
	// draw is inside a scene, and EndScene is only owed for the bracket this
	// opened.
	const bool openedScene = !d3d11::Failed(beginScene(gameDevice));

	struct ShadeVertex {
		float x, y, z, rhw;
		UInt32 color;
	};
	static_assert(sizeof(ShadeVertex) == 20, "XYZRHW|DIFFUSE is a 20-byte vertex");

	bool drewBoth = true;
	for (Eye& eye : m_eye) {
		setTarget(gameDevice, 0, eye.surface);
		setDepth(gameDevice, nullptr);

		const d3d9::Rect& r = trimToSharedWindow ? eye.commonDestination : eye.destination;
		const float left = static_cast<float>(r.left) - 0.5f;
		const float top = static_cast<float>(r.top) - 0.5f;
		const float right = static_cast<float>(r.right) - 0.5f;
		const float bottom = static_cast<float>(r.bottom) - 0.5f;
		const ShadeVertex quad[4] = {
			{left, top, 0.5f, 1.0f, shadeColorArgb},
			{right, top, 0.5f, 1.0f, shadeColorArgb},
			{left, bottom, 0.5f, 1.0f, shadeColorArgb},
			{right, bottom, 0.5f, 1.0f, shadeColorArgb},
		};

		if (d3d11::Failed(draw(gameDevice, d3d9::kPrimitiveTriangleStrip, 2, quad,
		                       sizeof(ShadeVertex)))) {
			drewBoth = false;
		}
	}

	if (openedScene) {
		endScene(gameDevice);
	}

	// The game's world first, then its states: SetRenderTarget resets the
	// viewport to the target's full size, and the state block's Apply is what
	// puts the real one back afterwards.
	setTarget(gameDevice, 0, previousTarget);
	setDepth(gameDevice, previousDepth);
	auto apply = d3d9::Method<d3d9::StateBlockMethodFn>(block, d3d9::kStateBlockApply);
	if (apply != nullptr) {
		apply(block);
	}
	d3d11::Release(block);
	d3d11::Release(previousTarget);
	d3d11::Release(previousDepth);

	if (!drewBoth && !m_shadeFailureLogged) {
		m_shadeFailureLogged = true;
		OBVR_LOG("Mirror: the menu shade quad was refused, so the held pair is only "
		         "part-shaded");
	}
	return drewBoth;
}

bool EyeMirror::BeginSubmit(void* gameDevice) {
	if (!IsReady()) {
		return false;
	}

	// Read before the flush, not after. What DXVK reports is where the image
	// will be once outstanding commands have run, so this is the answer that
	// the flush inside Begin then makes true - and it is different from the
	// answer at creation, because something has drawn into the image since.
	for (int index = 0; index < 2; ++index) {
		if (!ReadImageInfo(m_eye[index].interop, m_eye[index].image)) {
			return false;
		}
	}

	if (!m_bracket.Begin(gameDevice)) {
		return false;
	}

	for (int index = 0; index < 2; ++index) {
		if (!m_bracket.ToTransferSrc(m_eye[index].interop, m_eye[index].image.layout)) {
			// Nothing partial is left behind: releasing here undoes whichever
			// transition did happen, in reverse, and unlocks the queue.
			m_bracket.Release();
			return false;
		}
	}

	return true;
}

void EyeMirror::Destroy() {
	// The bracket first. It undoes layouts and unlocks the queue, and both of
	// those need the textures it is holding to still exist.
	m_bracket.Release();

	for (Eye& eye : m_eye) {
		d3d11::Release(eye.interop);
		d3d11::Release(eye.surface);
		d3d11::Release(eye.texture);
		eye.image = BackBufferImage{};
		eye.destination = d3d9::Rect{};
		eye.source = d3d9::Rect{};
	}

	m_width = 0;
	m_height = 0;
	m_frameWidth = 0;
	m_frameHeight = 0;
	m_format = 0;
	m_filter = 0;
	m_cropped = false;
	m_primed = false;
	m_lastWasFlat = false;
	m_everCopied = false;
	m_heldShaded = false;
}

}  // namespace obvr::render
