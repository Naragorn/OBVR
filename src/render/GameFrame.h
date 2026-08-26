#pragma once

#include "render/DxvkInterop.h"
#include "render/GameDevice.h"
#include "render/InteropBracket.h"

namespace obvr::render {

// Borrows Oblivion's finished frame for long enough to hand it to the
// compositor, and puts everything back afterwards.
//
// The holding-still half - flush, lock, transition, undo - is InteropBracket,
// because AER needs exactly the same six steps around two images it owns
// rather than one it borrows. What is left here is the borrowing: find the
// back buffer, see whether it could be submitted at all, and let go of it
// afterwards.
//
// Borrowed is the operative word, and it is why this class alone is not
// enough for stereo. The image behind it is the one the game draws the next
// frame into, so it is only a picture for as long as nothing else has
// happened. One eye per frame therefore cannot work by keeping the previous
// frame's back buffer around: there is no previous frame's back buffer, only
// the same buffer with newer pixels in it. See EyeMirror.
class GameFrame {
public:
	~GameFrame() { Release(); }

	GameFrame() = default;
	GameFrame(const GameFrame&) = delete;
	GameFrame& operator=(const GameFrame&) = delete;

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
	void* m_surface = nullptr;  // IDirect3DSurface9
	void* m_texture = nullptr;  // ID3D9VkInteropTexture

	BackBufferImage m_image;

	// Declared last so it is destroyed first: the queue has to be released
	// before the surface it was locked around goes away.
	InteropBracket m_bracket;
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
