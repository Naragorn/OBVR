#include "render/InteropBracket.h"

#include "render/D3D11Types.h"
#include "render/GameDevice.h"

namespace obvr::render {
namespace {

// The whole image: one mip level, one array layer, colour only. Neither a
// back buffer nor an eye copy has anything else.
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

bool InteropBracket::Begin(void* gameDevice) {
	Release();

	if (gameDevice == nullptr) {
		return false;
	}

	auto* unknown = static_cast<d3d11::Unknown*>(gameDevice);
	if (unknown->vtbl == nullptr || unknown->vtbl->QueryInterface == nullptr) {
		return false;
	}

	if (d3d11::Failed(
			unknown->vtbl->QueryInterface(unknown, &kIID_D3D9VkInteropDevice, &m_interop)) ||
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

	// Outstanding work has to reach the queue before the compositor reads
	// anything, and before this happens the layouts DXVK reported are still
	// only promises.
	interop->vtbl->FlushRenderingCommands(interop);

	// From here the queue is ours. Everything below must reach Release.
	interop->vtbl->LockSubmissionQueue(interop);
	m_queueLocked = true;
	return true;
}

bool InteropBracket::ToTransferSrc(void* interopTexture, UInt32 currentLayout) {
	if (!m_queueLocked || m_interop == nullptr || interopTexture == nullptr) {
		return false;
	}

	// Already where it needs to be. Not an error, and deliberately not an
	// undo either - putting an image "back" into a layout it was already in
	// would be a transition nobody asked for.
	if (currentLayout == dxvk::kImageLayoutTransferSrcOptimal) {
		return true;
	}

	if (m_undoCount >= kMaxTransitions) {
		return false;
	}

	auto* interop = static_cast<dxvk::InteropDevice*>(m_interop);
	const dxvk::VkImageSubresourceRange range = WholeImage();
	interop->vtbl->TransitionTextureLayout(interop, interopTexture, &range, currentLayout,
	                                       dxvk::kImageLayoutTransferSrcOptimal);

	m_undo[m_undoCount].texture = interopTexture;
	m_undo[m_undoCount].layout = currentLayout;
	++m_undoCount;
	return true;
}

void InteropBracket::Release() {
	auto* interop = static_cast<dxvk::InteropDevice*>(m_interop);

	// Reverse order, and still under the lock. A transition is queue work, so
	// undoing one after the queue has been released would be doing it without
	// the exclusion the first one was taken under.
	if (interop != nullptr) {
		for (int i = m_undoCount - 1; i >= 0; --i) {
			const dxvk::VkImageSubresourceRange range = WholeImage();
			interop->vtbl->TransitionTextureLayout(interop, m_undo[i].texture, &range,
			                                       dxvk::kImageLayoutTransferSrcOptimal,
			                                       m_undo[i].layout);
		}
	}
	m_undoCount = 0;

	if (m_queueLocked && interop != nullptr) {
		// Not optional and not best effort. A queue left locked deadlocks
		// Oblivion against its own renderer, and the symptom is a frozen game
		// with nothing in any log.
		interop->vtbl->ReleaseSubmissionQueue(interop);
	}
	m_queueLocked = false;

	d3d11::Release(m_interop);
}

}  // namespace obvr::render
