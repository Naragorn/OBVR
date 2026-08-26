#include "render/EyeMirror.h"

#include "core/Log.h"
#include "render/D3D11Types.h"
#include "render/D3D9Types.h"

namespace obvr::render {
namespace {

// Opaque black. The margin around Oblivion's frame, which the game never
// draws into and which would otherwise show whatever the memory held when the
// texture was made - in a headset, a border of noise around the world.
constexpr UInt32 kOpaqueBlack = 0xFF000000;

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
                       float gameFovDegrees, bool gameFovIsFor4x3) {
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

	// Where the game's frame goes inside each eye's view. This is what makes
	// the world its real size rather than whatever magnification two
	// unrelated frustums happen to imply.
	for (int index = 0; index < 2; ++index) {
		const EyeProjection& projection = index == 0 ? leftEye : rightEye;
		const PicturePlacement placement =
			PlacePicture(projection, gameFovDegrees, m_frameWidth, m_frameHeight,
			             gameFovIsFor4x3);

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

	OBVR_LOG("Mirror: the game's %ux%u frame at %.1f degrees sits at left x=%d..%d y=%d..%d, "
	         "right x=%d..%d y=%d..%d%s",
	         m_frameWidth, m_frameHeight, static_cast<double>(gameFovDegrees),
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
	         "black. Raise Oblivion's fDefaultFOV to fill more, at the cost of pushing the "
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

bool EyeMirror::CopyBackBuffer(void* gameDevice, bool isLeft) {
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

	// Both on the first pass. After that only the eye this frame belongs to -
	// the other one keeps the picture from its own last turn, which is what
	// makes the two eyes differ at all.
	const int first = m_primed ? (isLeft ? 0 : 1) : 0;
	const int last = m_primed ? first : 1;

	bool ok = true;
	for (int index = first; index <= last; ++index) {
		const Eye& eye = m_eye[index];
		SInt32 result =
			stretchRect(gameDevice, source, &eye.source, eye.surface, &eye.destination,
		                m_filter);

		// A device may refuse linear stretching - it is a capability, not a
		// guarantee. Point filtering is worse to look at and better than no
		// picture, and the fallback is remembered so the refusal costs one
		// extra call rather than one per frame for ever.
		if (d3d11::Failed(result) && m_filter != d3d9::kTexFilterPoint) {
			OBVR_LOG("Mirror: linear stretching was refused, falling back to point");
			m_filter = d3d9::kTexFilterPoint;
			result = stretchRect(gameDevice, source, &eye.source, eye.surface,
			                     &eye.destination, m_filter);
		}

		if (d3d11::Failed(result)) {
			ok = false;
		}
	}

	d3d11::Release(source);

	if (ok) {
		m_primed = true;
	}
	return ok;
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
}

}  // namespace obvr::render
