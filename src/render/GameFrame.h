#pragma once

#include "render/DxvkInterop.h"
#include "render/GameDevice.h"

namespace obvr::render {

// Borrows Oblivion's finished frame for long enough to hand it to the
// compositor, and puts everything back afterwards.
//
// One class rather than a handful of calls because the sequence is a bracket,
// not a list, and every step has an undo that must happen even when a later
// step fails:
//
//   flush outstanding commands   so the compositor does not read a half-drawn
//                                frame
//   lock the submission queue    because SteamVR will schedule work on DXVK's
//                                own queue, and Vulkan permits exactly one
//                                thread on a queue at a time
//   transition GENERAL to        because SteamVR requires that layout and a
//   TRANSFER_SRC_OPTIMAL         back buffer is not in it
//   ... submit ...
//   transition back              because DXVK expects to find its image the
//                                way it left it
//   release the queue            or the game deadlocks on its own renderer
//
// Getting the undo half wrong does not produce a wrong picture. It produces a
// frozen game, which is why the ordering lives in one place with one owner
// rather than spread across the caller.
class GameFrame {
public:
	~GameFrame() { Release(); }

	// Takes the back buffer and prepares it for submission. False if
	// anything is missing, in which case nothing is held and nothing needs
	// releasing.
	//
	// After this returns true the submission queue is locked. It stays
	// locked until Release, so the window between them should contain the
	// submit calls and nothing else.
	bool Acquire(void* gameDevice);

	// Puts the layout back, unlocks the queue and drops every reference.
	// Safe to call when nothing was acquired, and safe to call twice.
	void Release();

	bool IsAcquired() const { return m_texture != nullptr; }

	const BackBufferImage& GetImage() const { return m_image; }

private:
	void* m_surface = nullptr;   // IDirect3DSurface9
	void* m_texture = nullptr;   // ID3D9VkInteropTexture
	void* m_interop = nullptr;   // ID3D9VkInteropDevice

	BackBufferImage m_image;

	// Whether the layout was actually changed, and therefore whether it has
	// to be changed back. Not the same as having acquired: a frame already in
	// the right layout needs no transition and must not get an undo either.
	bool m_transitioned = false;

	// Whether the queue lock was taken. Kept apart from the transition flag
	// because the two are undone in opposite order and a single flag would
	// eventually be used for both.
	bool m_queueLocked = false;
};

// Fills in the structure OpenVR wants from a frame and the device it came
// from.
//
// Separated out so it can be checked without either: it is ten fields copied
// from two places, and the failure mode of copying one from the wrong place
// is a compositor error that names none of them.
void DescribeForOpenVR(const BackBufferImage& image, const VulkanContext& context,
                       dxvk::VRVulkanTextureData& out);

}  // namespace obvr::render
