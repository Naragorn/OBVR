#pragma once

#include "core/Types.h"
#include "render/DxvkInterop.h"

namespace obvr::render {

// Holds DXVK still for long enough that the compositor can read from its
// images, and puts everything back afterwards.
//
// The sequence is a bracket rather than a list, and every step has an undo
// that has to happen even when a later step fails:
//
//   flush outstanding commands   so the compositor does not read a half-drawn
//                                frame - the layout DXVK reports is a promise
//                                about after the flush, so the flush is what
//                                makes it true
//   lock the submission queue    because SteamVR schedules work on DXVK's own
//                                queue, and Vulkan permits exactly one thread
//                                on a queue at a time
//   transition to                because SteamVR requires that layout and
//   TRANSFER_SRC_OPTIMAL         nothing DXVK hands out is in it
//   ... submit ...
//   transition back              because DXVK expects to find its images the
//                                way it left them
//   release the queue            or the game deadlocks against its own renderer
//
// One class with one owner rather than the same six steps written out at each
// call site. Getting the undo half wrong does not produce a wrong picture: it
// produces a frozen game with an empty log, and a second nearly-identical copy
// of this logic is exactly how one of the two ends up missing a step.
class InteropBracket {
public:
	~InteropBracket() { Release(); }

	InteropBracket() = default;
	InteropBracket(const InteropBracket&) = delete;
	InteropBracket& operator=(const InteropBracket&) = delete;

	// Takes the interop device, flushes, and locks the queue. False if the
	// device is not DXVK or the interface is missing a method OBVR needs, in
	// which case nothing is held and nothing needs releasing.
	//
	// After this returns true the queue is locked. It stays locked until
	// Release, so the window between them should contain the submit calls and
	// as little else as possible.
	bool Begin(void* gameDevice);

	// Moves one image into the layout SteamVR wants, and records how to put
	// it back. currentLayout is what DXVK reported for that image; an image
	// already in the right layout is left alone and gets no undo either.
	//
	// False if the queue is not held, or if more images are offered than
	// there is room to undo - never silently, because a transition without a
	// matching undo leaves DXVK's own image in a layout it does not expect.
	bool ToTransferSrc(void* interopTexture, UInt32 currentLayout);

	// Undoes every transition, in reverse, while the queue is still held -
	// which is the only ordering that is right rather than merely plausible,
	// since a transition is itself queue work. Then unlocks and drops the
	// device. Safe when nothing was begun, and safe to call twice.
	void Release();

	bool IsHeld() const { return m_queueLocked; }

private:
	// Two, because that is one image per eye and nothing here has ever wanted
	// a third. A fixed array rather than a container so this file needs no
	// allocator on a path that runs once per frame.
	static constexpr int kMaxTransitions = 2;

	struct Undo {
		void* texture;
		UInt32 layout;
	};

	void* m_interop = nullptr;  // ID3D9VkInteropDevice
	Undo m_undo[kMaxTransitions] = {};
	int m_undoCount = 0;
	bool m_queueLocked = false;
};

}  // namespace obvr::render
