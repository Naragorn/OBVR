#include "render/EyeMirror.h"

#include "core/Log.h"
#include "render/D3D11Types.h"
#include "render/D3D9Types.h"

namespace obvr::render {
namespace {

// Reads the back buffer's size and format, so the copies match it exactly.
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

bool EyeMirror::Create(void* gameDevice) {
	Destroy();

	if (gameDevice == nullptr) {
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

	m_width = desc.width;
	m_height = desc.height;
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
	OBVR_LOG("Mirror: %ux%u D3DFORMAT %u, Vulkan format %u, usage 0x%08X - %s", m_width,
	         m_height, m_format, left.format, left.usage,
	         IsSubmittableImage(left) ? "submittable"
	                                  : "NOT submittable, so alternate eyes cannot work");

	if (!IsSubmittableImage(left) || !IsSubmittableImage(m_eye[1].image)) {
		OBVR_LOG("Mirror: SteamVR requires TRANSFER_SRC (0x1) and SAMPLED (0x4) on a "
		         "submitted image, and one of them is missing");
		Destroy();
		return false;
	}

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

	// Both on the first pass. After that only the eye this frame was drawn
	// for - the other one keeps the picture from its own last turn, which is
	// what makes the two eyes differ at all.
	const int first = m_primed ? (isLeft ? 0 : 1) : 0;
	const int last = m_primed ? first : 1;

	bool ok = true;
	for (int index = first; index <= last; ++index) {
		// No rectangles and no filter: same size in and out, so this is a
		// copy rather than a resize, and asking for a filter would be asking
		// the GPU to resample a picture into itself.
		if (d3d11::Failed(stretchRect(gameDevice, source, nullptr, m_eye[index].surface,
		                              nullptr, d3d9::kTexFilterNone))) {
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
	}

	m_width = 0;
	m_height = 0;
	m_format = 0;
	m_primed = false;
}

}  // namespace obvr::render
