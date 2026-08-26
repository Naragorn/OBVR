#pragma once

#include "core/Types.h"
#include "render/D3D9Types.h"
#include "render/DxvkInterop.h"
#include "render/EyeGeometry.h"
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

	// Makes both pictures at the size the headset asked for, works out where
	// Oblivion's frame belongs inside each of them, and blacks out the rest.
	//
	// The texture is the eye's whole field of view; the game's frame occupies
	// only the part of it the game's own field of view is entitled to. That
	// is the difference between a world at its real size and a world magnified
	// by whatever ratio two unrelated frustums happen to have.
	//
	// CreateTexture rather than CreateRenderTarget, and the reason is in
	// D3D9Types.h: in DXVK only the former produces an image carrying
	// VK_IMAGE_USAGE_SAMPLED_BIT, SteamVR requires it, and usage is settled
	// when an image is created. A surface from CreateRenderTarget would be
	// perfectly good to draw into and impossible to submit.
	bool Create(void* gameDevice, UInt32 textureWidth, UInt32 textureHeight,
	            const EyeProjection& leftEye, const EyeProjection& rightEye,
	            float gameFovDegrees, bool gameFovIsFor4x3, float cameraTanHalfWidth,
	            float cameraTanHalfHeight, float menuScale, float menuAspect);

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
	// bothEyes fills both copies from the same picture, which is what a menu
	// wants: there is no camera and no eye separation, so anything else would
	// be inventing depth that is not there.
	bool CopyBackBuffer(void* gameDevice, bool isLeft, bool bothEyes = false);

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

	// Whether any part of Oblivion's frame had to be cut away to fit.
	bool WasCropped() const { return m_cropped; }

private:
	bool BeginSubmit(void* gameDevice);
	void EndSubmit() { m_bracket.Release(); }

	bool CreateOne(void* gameDevice, int index);

	struct Eye {
		void* texture = nullptr;  // IDirect3DTexture9
		void* surface = nullptr;  // IDirect3DSurface9, level 0 of that texture
		void* interop = nullptr;  // ID3D9VkInteropTexture
		BackBufferImage image;

		// Where Oblivion's frame is copied to inside this eye's texture, and
		// which part of the frame is used. Worked out once, because neither
		// the eye's frustum nor the game's can change within a run.
		d3d9::Rect destination = {};
		d3d9::Rect source = {};

		// Where a flat picture goes: menus, videos, loading screens. Smaller
		// and centred, so it reads as a screen in front of the wearer rather
		// than as the world.
		d3d9::Rect flatDestination = {};
	};

	// Index 0 is the left eye, 1 the right. Which eye a frame belongs to is
	// camera::IsLeftEyeFrame and nothing here decides it independently - the
	// camera hook has already moved the camera by the time this runs, and the
	// two disagreeing would show each eye the other's viewpoint.
	Eye m_eye[2];

	// The eye texture's size - the eye's whole field of view.
	UInt32 m_width = 0;
	UInt32 m_height = 0;

	// Oblivion's own frame, which covers only part of that. Kept because the
	// source rectangle is in its coordinates, not the texture's.
	UInt32 m_frameWidth = 0;
	UInt32 m_frameHeight = 0;

	// Which slice of the frame a flat picture takes, so a cinema shape is a
	// crop rather than a squeeze.
	d3d9::Rect m_flatSource = {};

	UInt32 m_format = 0;  // D3DFORMAT, taken from the back buffer

	// The filter StretchRect is given. Linear, because the copy scales now -
	// but a device that refuses linear stretching falls back to point rather
	// than losing the picture.
	UInt32 m_filter = 0;

	// Whether any part of Oblivion's frame had to be cut away to fit. Only
	// happens when the game renders wider than the headset can show.
	bool m_cropped = false;

	// Whether the last copy was a flat one. A change either way needs the
	// margin blacking out again, because the two placements are different
	// sizes and the larger leaves pixels outside the smaller.
	bool m_lastWasFlat = false;
	bool m_everCopied = false;

	// Whether both pictures have ever been written to.
	bool m_primed = false;

	InteropBracket m_bracket;
};

}  // namespace obvr::render
