#include "render/GameFrame.h"

#include "core/Log.h"
#include "render/D3D11Types.h"
#include "render/D3D9Types.h"

namespace obvr::render {

void DescribeForOpenVR(const BackBufferImage& image, const VulkanContext& context,
                       dxvk::VRVulkanTextureData& out) {
	out.image = image.image;
	out.device = context.device;
	out.physicalDevice = context.physicalDevice;
	out.instance = context.instance;
	out.queue = context.queue;

	// The family index, not the queue index. They are different numbers with
	// similar names and the compositor cannot tell that the wrong one was
	// passed - it would simply schedule work against a family that does not
	// own the queue.
	out.queueFamilyIndex = context.queueFamilyIndex;

	out.width = image.width;
	out.height = image.height;
	out.format = image.format;
	out.sampleCount = image.sampleCount;
}


bool FrameResolve::Create(void* gameDevice, const d3d9::SurfaceDesc& desc) {
	Destroy();

	auto createTexture =
		d3d9::Method<d3d9::CreateTextureFn>(gameDevice, d3d9::kDeviceCreateTexture);
	if (createTexture == nullptr ||
	    d3d11::Failed(createTexture(gameDevice, desc.width, desc.height, 1,
	                                d3d9::kUsageRenderTarget, desc.format, d3d9::kPoolDefault,
	                                &m_texture, nullptr)) ||
	    m_texture == nullptr) {
		m_texture = nullptr;
		return false;
	}

	auto getSurfaceLevel =
		d3d9::Method<d3d9::GetSurfaceLevelFn>(m_texture, d3d9::kTextureGetSurfaceLevel);
	if (getSurfaceLevel == nullptr ||
	    d3d11::Failed(getSurfaceLevel(m_texture, 0, &m_surface)) || m_surface == nullptr) {
		m_surface = nullptr;
		Destroy();
		return false;
	}

	m_width = desc.width;
	m_height = desc.height;
	m_format = desc.format;
	return true;
}

void* FrameResolve::ResolveFrom(void* gameDevice, void* backBuffer,
                                const d3d9::SurfaceDesc& desc) {
	if (gameDevice == nullptr || backBuffer == nullptr) {
		return nullptr;
	}

	if (m_surface == nullptr || m_width != desc.width || m_height != desc.height ||
	    m_format != desc.format) {
		if (!Create(gameDevice, desc)) {
			if (!m_failureLogged) {
				m_failureLogged = true;
				OBVR_LOG("Render: the back buffer has %u samples and a single-sample copy of "
				         "it could not be created, so it cannot be submitted",
				         desc.multiSampleType);
			}
			return nullptr;
		}
	}

	// Whole surface to whole surface with no filter: that is the shape DXVK
	// resolves directly rather than through a blit.
	auto stretchRect = d3d9::Method<d3d9::StretchRectFn>(gameDevice, d3d9::kDeviceStretchRect);
	if (stretchRect == nullptr ||
	    d3d11::Failed(stretchRect(gameDevice, backBuffer, nullptr, m_surface, nullptr,
	                              d3d9::kTexFilterNone))) {
		if (!m_failureLogged) {
			m_failureLogged = true;
			OBVR_LOG("Render: the back buffer has %u samples and the device refused to "
			         "resolve it into a single-sample copy, so it cannot be submitted",
			         desc.multiSampleType);
		}
		return nullptr;
	}

	if (!m_reported) {
		m_reported = true;
		OBVR_LOG("Render: the back buffer has %u samples (the game's antialiasing), so each "
		         "frame is resolved into a %ux%u single-sample copy before it is submitted",
		         desc.multiSampleType, m_width, m_height);
	}

	auto* unknown = static_cast<d3d11::Unknown*>(m_surface);
	if (unknown->vtbl != nullptr && unknown->vtbl->AddRef != nullptr) {
		unknown->vtbl->AddRef(unknown);
	}
	return m_surface;
}

void FrameResolve::Destroy() {
	d3d11::Release(m_surface);
	d3d11::Release(m_texture);
	m_width = 0;
	m_height = 0;
	m_format = 0;
}

bool GameFrame::Acquire(void* gameDevice, FrameResolve* resolve) {
	Release();

	if (gameDevice == nullptr) {
		return false;
	}

	auto getBackBuffer =
		d3d9::Method<d3d9::GetBackBufferFn>(gameDevice, d3d9::kDeviceGetBackBuffer);
	if (getBackBuffer == nullptr ||
	    d3d11::Failed(
			getBackBuffer(gameDevice, 0, 0, d3d9::kBackBufferTypeMono, &m_surface)) ||
	    m_surface == nullptr) {
		m_surface = nullptr;
		Release();
		return false;
	}

	// A multisampled back buffer is not a picture the compositor takes. With
	// a resolve to hand, its single-sample copy stands in; the copy is what
	// gets locked and transitioned below, and the back buffer itself is let
	// go of here.
	if (resolve != nullptr) {
		d3d9::SurfaceDesc desc{};
		auto getDesc = d3d9::Method<d3d9::GetDescFn>(m_surface, d3d9::kSurfaceGetDesc);
		if (getDesc != nullptr && !d3d11::Failed(getDesc(m_surface, &desc)) &&
		    desc.multiSampleType != d3d9::kMultiSampleNone) {
			void* resolved = resolve->ResolveFrom(gameDevice, m_surface, desc);
			d3d11::Release(m_surface);
			m_surface = resolved;
			if (m_surface == nullptr) {
				Release();
				return false;
			}
		}
	}

	auto* surface = static_cast<d3d11::Unknown*>(m_surface);
	if (surface->vtbl == nullptr || surface->vtbl->QueryInterface == nullptr ||
	    d3d11::Failed(surface->vtbl->QueryInterface(surface, &kIID_D3D9VkInteropTexture,
	                                                &m_texture)) ||
	    m_texture == nullptr) {
		m_texture = nullptr;
		Release();
		return false;
	}

	if (!ReadImageInfo(m_texture, m_image)) {
		Release();
		return false;
	}

	// Checked before anything is held, so a frame that could never be
	// submitted costs no lock and no transition to undo.
	if (!IsSubmittableImage(m_image)) {
		Release();
		return false;
	}

	if (!m_bracket.Begin(gameDevice)) {
		Release();
		return false;
	}

	m_bracket.ToTransferSrc(m_texture, m_image.layout);
	return true;
}

void GameFrame::Release() {
	// The bracket first: it undoes the layout and unlocks the queue, and both
	// of those need the surface still to exist.
	m_bracket.Release();

	d3d11::Release(m_texture);
	d3d11::Release(m_surface);

	m_image = BackBufferImage{};
}

}  // namespace obvr::render
