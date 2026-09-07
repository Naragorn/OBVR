#pragma once

#include "render/D3D9Types.h"
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

// A single-sample copy of the back buffer, for the frames where the back
// buffer itself has more than one sample.
//
// Oblivion's launcher offers antialiasing, and with it on the swap chain's
// back buffer is a multisampled image. SteamVR's Vulkan path does not take
// one: submitting it is refused, and the refusal reaches the log as 105
// (TextureUsesUnsupportedFormat) on the eye that completes the frame, which
// the policy rightly treats as permanent. So with 8x antialiasing the mono
// submit of the first world frame ended the session in the headset while the
// monitor played on - the first outside report after 0.1.2, a Quest 3. The
// eye copies never had the problem: StretchRect into a single-sample texture
// is a resolve and DXVK performs it. This is the same resolve, whole surface
// into a texture of the frame's size and format, kept for the life of the
// device and refreshed each time a frame is acquired through it.
//
// A texture with D3DUSAGE_RENDERTARGET rather than a surface from
// CreateRenderTarget, for the reason recorded at EyeMirror::CreateOne: only
// the texture comes out of DXVK carrying SAMPLED, and the compositor needs it.
class FrameResolve {
public:
	~FrameResolve() { Destroy(); }

	FrameResolve() = default;
	FrameResolve(const FrameResolve&) = delete;
	FrameResolve& operator=(const FrameResolve&) = delete;

	// Copies the given back buffer into the single-sample texture, creating
	// or re-creating that texture when the size or format differ, and
	// returns the resolved surface with a reference the caller owns. Null
	// when the copy could not be made; the reason is logged once.
	void* ResolveFrom(void* gameDevice, void* backBuffer, const d3d9::SurfaceDesc& desc);

	void Destroy();

private:
	bool Create(void* gameDevice, const d3d9::SurfaceDesc& desc);

	void* m_texture = nullptr;  // IDirect3DTexture9
	void* m_surface = nullptr;  // IDirect3DSurface9, level 0
	UInt32 m_width = 0;
	UInt32 m_height = 0;
	UInt32 m_format = 0;
	bool m_failureLogged = false;
	bool m_reported = false;
};

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
	// When the back buffer is multisampled and a resolve is given, its
	// single-sample copy is taken in the back buffer's place; without one a
	// multisampled back buffer is refused, as it always was.
	//
	// After this returns true the submission queue is locked. It stays
	// locked until Release, so the window between them should contain the
	// submit calls and nothing else.
	bool Acquire(void* gameDevice, FrameResolve* resolve = nullptr);

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
