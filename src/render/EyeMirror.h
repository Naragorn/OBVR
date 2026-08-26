#pragma once

#include "core/Types.h"
#include "render/DxvkInterop.h"
#include "render/GameDevice.h"
#include "render/InteropBracket.h"

namespace obvr::render {

// Two pictures OBVR owns, one per eye, each holding the last frame that was
// drawn for that eye.
//
// This exists because of one measured fact: the compositor counts a frame as
// delivered only when both eyes have been submitted. The first attempt at
// alternate eye rendering submitted one eye per frame and let the compositor
// hold the other eye's last texture. Every call reported success, no error of
// any kind reached the log, and SteamVR faded to Home after about ten frames -
// which is what it documents doing when an application stops delivering. So a
// single eye is a successful call and not a frame. The evidence is
// docs/verification/OBVR-aer-refused.log, and it says so by omission: there is
// nothing in it.
//
// Both eyes therefore have to be submitted every frame, which means the eye
// that was not drawn this time still needs a picture of its own. It cannot be
// the back buffer, because there is only one of those and the game is about to
// draw the next frame into it. Hence a copy per eye, owned here, alive for as
// long as OBVR wants it.
//
// What this costs is one full-screen GPU copy per frame. What it buys is that
// the two eyes hold pictures drawn from two different camera positions, which
// is the whole point.
class EyeMirror {
public:
	~EyeMirror() { Destroy(); }

	EyeMirror() = default;
	EyeMirror(const EyeMirror&) = delete;
	EyeMirror& operator=(const EyeMirror&) = delete;

	// Makes both pictures, matching the back buffer in size and format.
	//
	// CreateTexture rather than CreateRenderTarget, and the reason is in
	// D3D9Types.h: in DXVK only the former produces an image carrying
	// VK_IMAGE_USAGE_SAMPLED_BIT, SteamVR requires it, and usage is settled
	// when an image is created. A surface from CreateRenderTarget would be
	// perfectly good to draw into and impossible to submit.
	bool Create(void* gameDevice);

	void Destroy();

	bool IsReady() const { return m_eye[0].interop != nullptr && m_eye[1].interop != nullptr; }

	// Copies what the game has just drawn into one eye's own picture. Pure
	// Direct3D 9 - it must be called with no submission queue held, because
	// it puts work on the very queue the bracket locks.
	//
	// The first call fills both eyes rather than one. Otherwise the eye that
	// is not drawn first would be submitted once holding whatever an
	// uninitialised render target happens to contain, which is a flash of
	// something in one eye at exactly the moment the wearer is looking for
	// whether this works at all.
	bool CopyBackBuffer(void* gameDevice, bool isLeft);

	// Brackets both pictures for submission: re-reads their layouts, flushes,
	// locks the queue and moves both into TRANSFER_SRC_OPTIMAL.
	//
	// Held through this rather than through a pair of calls, because the
	// undo is not optional. A path that returns early with the queue locked
	// freezes Oblivion against its own renderer, and the log stays empty.
	class Submission {
	public:
		Submission(EyeMirror& mirror, void* gameDevice) : m_mirror(mirror) {
			m_held = mirror.BeginSubmit(gameDevice);
		}
		~Submission() { m_mirror.EndSubmit(); }

		Submission(const Submission&) = delete;
		Submission& operator=(const Submission&) = delete;

		bool IsHeld() const { return m_held; }

	private:
		EyeMirror& m_mirror;
		bool m_held = false;
	};

	const BackBufferImage& GetImage(bool isLeft) const { return m_eye[isLeft ? 0 : 1].image; }

	UInt32 GetWidth() const { return m_width; }
	UInt32 GetHeight() const { return m_height; }

private:
	bool BeginSubmit(void* gameDevice);
	void EndSubmit() { m_bracket.Release(); }

	bool CreateOne(void* gameDevice, int index);

	struct Eye {
		void* texture = nullptr;  // IDirect3DTexture9
		void* surface = nullptr;  // IDirect3DSurface9, level 0 of that texture
		void* interop = nullptr;  // ID3D9VkInteropTexture
		BackBufferImage image;
	};

	// Index 0 is the left eye, 1 the right. Which eye a frame belongs to is
	// camera::IsLeftEyeFrame and nothing here decides it independently - the
	// camera hook has already moved the camera by the time this runs, and the
	// two disagreeing would show each eye the other's viewpoint.
	Eye m_eye[2];

	UInt32 m_width = 0;
	UInt32 m_height = 0;
	UInt32 m_format = 0;  // D3DFORMAT, taken from the back buffer

	// Whether both pictures have ever been written to.
	bool m_primed = false;

	InteropBracket m_bracket;
};

}  // namespace obvr::render
