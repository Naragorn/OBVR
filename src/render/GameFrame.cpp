#include "render/GameFrame.h"

#include "core/Log.h"
#include "render/D3D11Types.h"

namespace obvr::render {
namespace {

// The whole image: one mip level, one array layer, colour only. A back buffer
// has nothing else.
dxvk::VkImageSubresourceRange WholeImage() {
	dxvk::VkImageSubresourceRange range{};
	range.aspectMask = dxvk::kImageAspectColor;
	range.baseMipLevel = 0;
	range.levelCount = 1;
	range.baseArrayLayer = 0;
	range.layerCount = 1;
	return range;
}

}  // namespace

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

	auto* unknown = static_cast<d3d11::Unknown*>(gameDevice);
	if (unknown->vtbl == nullptr || unknown->vtbl->QueryInterface == nullptr) {
		return false;
	}

	// The interop device first. Without it none of the rest is possible, and
	// failing here costs nothing to undo.
	if (d3d11::Failed(unknown->vtbl->QueryInterface(unknown, &kIID_D3D9VkInteropDevice,
	                                                &m_interop)) ||
	    m_interop == nullptr) {
		m_interop = nullptr;
		return false;
	}

	auto* interop = static_cast<dxvk::InteropDevice*>(m_interop);
	if (interop->vtbl == nullptr || interop->vtbl->FlushRenderingCommands == nullptr ||
	    interop->vtbl->LockSubmissionQueue == nullptr ||
	    interop->vtbl->ReleaseSubmissionQueue == nullptr ||
	    interop->vtbl->TransitionTextureLayout == nullptr) {
		Release();
		return false;
	}

	using GetBackBufferFn = d3d11::ResultCode(__stdcall*)(void* self, UInt32 swapChain,
	                                                      UInt32 backBuffer, UInt32 type,
	                                                      void** surface);
	auto** vtbl = *reinterpret_cast<void***>(gameDevice);
	if (vtbl == nullptr) {
		Release();
		return false;
	}

	auto getBackBuffer =
		reinterpret_cast<GetBackBufferFn>(vtbl[dxvk::kD3D9GetBackBufferIndex]);
	if (getBackBuffer == nullptr ||
	    d3d11::Failed(
			getBackBuffer(gameDevice, 0, 0, dxvk::kBackBufferTypeMono, &m_surface)) ||
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

	auto* texture = static_cast<dxvk::InteropTexture*>(m_texture);
	if (texture->vtbl == nullptr || texture->vtbl->GetVulkanImageInfo == nullptr) {
		Release();
		return false;
	}

	UInt32 families[8] = {0};
	dxvk::VkImageCreateInfo info{};
	info.sType = dxvk::kStructureTypeImageCreateInfo;
	info.queueFamilyIndexCount = 0;
	info.pQueueFamilyIndices = families;

	unsigned long long image = 0;
	UInt32 layout = 0;
	if (d3d11::Failed(texture->vtbl->GetVulkanImageInfo(texture, &image, &layout, &info)) ||
	    image == 0) {
		Release();
		return false;
	}

	m_image.image = image;
	m_image.layout = layout;
	m_image.format = info.format;
	m_image.width = info.extent.width;
	m_image.height = info.extent.height;
	m_image.sampleCount = info.samples;
	m_image.usage = info.usage;

	// Checked before anything is changed, so a frame that could never be
	// submitted does not get a transition it would then need undoing.
	if (!IsSubmittableImage(m_image)) {
		Release();
		return false;
	}

	// Outstanding work has to reach the queue first. The layout DXVK reported
	// is what the image will be in once commands are flushed - a promise
	// about the future, so the flush is what makes it true.
	interop->vtbl->FlushRenderingCommands(interop);

	// From here the queue is ours. Everything below must reach Release.
	interop->vtbl->LockSubmissionQueue(interop);
	m_queueLocked = true;

	if (m_image.layout != dxvk::kImageLayoutTransferSrcOptimal) {
		const dxvk::VkImageSubresourceRange range = WholeImage();
		interop->vtbl->TransitionTextureLayout(interop, m_texture, &range, m_image.layout,
		                                       dxvk::kImageLayoutTransferSrcOptimal);
		m_transitioned = true;
	}

	return true;
}

void GameFrame::Release() {
	auto* interop = static_cast<dxvk::InteropDevice*>(m_interop);

	// Undone in the reverse of the order it was done, which is the only
	// ordering that is right rather than merely plausible: the transition
	// happens on the queue, so the queue has to still be held while it is
	// reversed.
	if (m_transitioned && interop != nullptr && m_texture != nullptr) {
		const dxvk::VkImageSubresourceRange range = WholeImage();
		interop->vtbl->TransitionTextureLayout(interop, m_texture, &range,
		                                       dxvk::kImageLayoutTransferSrcOptimal,
		                                       m_image.layout);
	}
	m_transitioned = false;

	if (m_queueLocked && interop != nullptr) {
		// Not optional and not best effort. A queue left locked deadlocks
		// Oblivion against its own renderer, and the symptom is a frozen game
		// with nothing in any log.
		interop->vtbl->ReleaseSubmissionQueue(interop);
	}
	m_queueLocked = false;

	d3d11::Release(m_texture);
	d3d11::Release(m_surface);
	d3d11::Release(m_interop);

	m_image = BackBufferImage{};
}

}  // namespace obvr::render
