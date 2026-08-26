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


bool GameFrame::Acquire(void* gameDevice) {
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
