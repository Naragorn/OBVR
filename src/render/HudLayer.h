#pragma once

#include "core/Types.h"
#include "render/D3D9Types.h"
#include "render/GameDevice.h"
#include "render/InteropBracket.h"
#include "vr/OpenVRTypes.h"

namespace obvr::vr {
class OpenVRBackend;
}

namespace obvr::render {

// The 2D layer's own picture, and the overlay it hangs from.
//
// This exists because of where the eye pictures are captured: after tone
// mapping and before the interface draws, so the world reaches the headset
// clean and the HUD does not reach it at all. Redirecting the interface pass
// into a texture of OBVR's own - complete with the alpha the back buffer
// never had - and handing that texture to IVROverlay puts the HUD back, as a
// quad the compositor places and reprojects itself.
//
// The texture is the size of the game's frame, because that is the layout
// space the game draws its interface in, cursor included - and keeping it
// one-to-one is what keeps a future look-at-the-menu cursor honest.
class HudLayer {
public:
	~HudLayer() { Destroy(); }

	HudLayer() = default;
	HudLayer(const HudLayer&) = delete;
	HudLayer& operator=(const HudLayer&) = delete;

	// Called at the entry of a redirected interface pass. Creates the texture
	// on first use (at the back buffer's size, A8R8G8B8), clears it to
	// transparent black, switches the alpha side of blending to the correct
	// over-operator, and hands back the surface the pass should draw into.
	//
	// frameNumber decides the clear: once per frame, not once per call. The
	// pass can run more than once in a frame, and clearing on every call
	// wiped the earlier call's drawing - twenty-two draws counted, and a
	// texture of nothing at Present, because the last caller of the frame
	// drew nothing over a fresh clear.
	//
	// Null when anything is missing - the submit then falls back to hiding
	// the overlay, and the pass draws into the back buffer as it always did.
	void* BeginCapture(void* gameDevice, UInt32 frameNumber);

	// Called after the redirected pass returned: puts the four blend states
	// back the way the game had them.
	void EndCapture();

	// Whether BeginCapture succeeded since the last Submit.
	bool HasCapture() const { return m_captured; }

	// The capture texture's surface and size, for the layout probe: which
	// rectangle of the texture the pass actually drew into is measured on the
	// texture itself, and the probe lives with the other readback plumbing
	// rather than in here.
	void* CaptureSurface() const { return m_surface; }
	void* CaptureTexture() const { return m_texture; }
	UInt32 CaptureWidth() const { return m_width; }
	UInt32 CaptureHeight() const { return m_height; }

	// Called at the end of the frame, after the eyes have been submitted.
	//
	// captured true: hands this frame's picture to the overlay and shows it,
	// creating the overlay on first use. captured false: hides it, so a
	// stale HUD does not hang in front of a menu the flat path is showing.
	//
	// The overlay's transform and width are set once, from the configuration,
	// when the overlay is created.
	//
	// probeSquare paints an opaque red square into the middle of the texture
	// just before it is handed over. It is the instrument for a HUD that
	// arrives as nothing: if the square reaches the headset, the overlay path
	// works and what is missing is the layer's own alpha; if even the square
	// does not, the display path itself is at fault. Hot reloaded through
	// Debug.HudProbe.
	// anchorWorld hangs the quad in the tracking space instead of on the
	// head, taking its anchor the first time a pose can be read and keeping
	// it until ResetAnchor. Switching it off mid-session puts the
	// head-relative transform back rather than leaving the quad in the room.
	void Submit(vr::OpenVRBackend& backend, void* gameDevice, bool captured,
	            float distanceMetres, float widthMetres, bool anchorWorld, bool probeSquare);

	// Drops the room anchor, so the next submit takes a fresh one from where
	// the head is now. This is what the recenter key means for a HUD that
	// hangs in the room: bring it back in front of me.
	void ResetAnchor() { m_anchorValid = false; }

	void Destroy();

private:
	bool EnsureTexture(void* gameDevice);
	bool EnsureOverlay(vr::OpenVRBackend& backend, float distanceMetres, float widthMetres);
	void PlaceInRoom(vr::OpenVRBackend& backend, float distanceMetres);

	void* m_texture = nullptr;  // IDirect3DTexture9
	void* m_surface = nullptr;  // IDirect3DSurface9, level 0
	void* m_interop = nullptr;  // ID3D9VkInteropTexture
	BackBufferImage m_image;

	UInt32 m_width = 0;
	UInt32 m_height = 0;
	bool m_textureTried = false;

	// The five states BeginCapture changes, in the order they are restored.
	UInt32 m_savedStates[5] = {};
	bool m_statesSaved = false;
	bool m_statesReported = false;
	bool m_captured = false;

	// The frame the texture was last cleared for. Not an optimisation: see
	// BeginCapture.
	UInt32 m_lastClearFrame = 0;
	bool m_everCleared = false;

	vr::openvr::VROverlayHandle m_overlay = vr::openvr::kOverlayHandleInvalid;
	bool m_overlayTried = false;
	bool m_overlayVisible = false;

	// The room anchor: a levelled head pose, held from when the layer was
	// last placed. Held rather than read per frame - see PlaceInRoom.
	vr::openvr::HmdMatrix34 m_anchorPose = {};
	bool m_anchorValid = false;
	bool m_anchorReported = false;
	UInt32 m_reanchorsReported = 0;  // self-heal log budget - see PlaceInRoom

	VulkanContext m_vulkan;
	bool m_vulkanChecked = false;
	bool m_vulkanUsable = false;

	bool m_liveReported = false;
	bool m_failureReported = false;

	// One CPU readback of the texture, a hundred captures in, while the probe
	// is on: the numbers that say whether the layer's content is there at all
	// and what its alpha looks like. See DumpContentOnce.
	UInt32 m_captureCount = 0;
	bool m_contentDumped = false;

	void DumpContentOnce(void* gameDevice);

	InteropBracket m_bracket;
};

}  // namespace obvr::render
