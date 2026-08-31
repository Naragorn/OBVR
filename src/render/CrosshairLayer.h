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

// The crosshair, as a quad of its own at the depth it is aiming at.
//
// This exists because of a fact about eyes rather than about rendering. The
// whole 2D interface reaches the headset as one flat overlay hung at a fixed
// distance (HudLayer), and the crosshair rides along in it. Anything that is
// not in the plane the eyes are converged on is seen twice - so a crosshair
// two metres away, laid over a target ten metres away, is physiologically
// doubled. Nothing is drawn wrong; there is simply one crosshair at the wrong
// depth, and the fix is to put it at the right one rather than to draw it
// differently.
//
// So the crosshair gets its own overlay, placed straight ahead at a distance
// this class is told.
//
// What hangs there is Oblivion's own crosshair, lifted out of the captured 2D
// layer and erased where it came from - not a copy drawn here. An earlier
// version did draw one, out of coloured rectangles, and it looked fine and was
// still wrong: the crosshair Oblivion draws is the context-sensitive one, so
// the hand, the lock and the speech icons come with it and change as the
// player looks around, and nothing here has to know what any of them mean.
//
// There is no drawn fallback left. It is the game's crosshair or none, which
// is also the honest failure: a hand-drawn near-miss reads as the game getting
// it wrong rather than as the mod being off.
class CrosshairLayer {
public:
	~CrosshairLayer() { Destroy(); }

	CrosshairLayer() = default;
	CrosshairLayer(const CrosshairLayer&) = delete;
	CrosshairLayer& operator=(const CrosshairLayer&) = delete;

	// Once per frame, after the eyes have been submitted.
	//
	// visible false hides the overlay - a menu is up, the world is not being
	// drawn, or the feature is off - so a crosshair does not hang in front of
	// an inventory the player is reading.
	//
	// distanceMetres and widthMetres arrive already decided - see
	// PlaceCrosshair, which clamps them and works the width out from the
	// distance so the crosshair keeps its apparent size. Nothing here
	// recomputes either: one place decides, and it is the one with tests.
	void Submit(vr::OpenVRBackend& backend, void* gameDevice, bool visible,
	            float distanceMetres, float widthMetres);

	// Takes Oblivion's own crosshair out of the captured 2D layer and into
	// this one - copies the middle of the layer here, then clears it there.
	//
	// Both halves are one operation, which is why they are one call: what is
	// lifted out has to be erased where it came from, or the flat crosshair
	// stays in the HUD quad at its wrong depth and there are two again.
	//
	// Worth the trouble over the drawn cross for a reason beyond looking
	// right: the crosshair Oblivion draws is the context-sensitive one, so the
	// hand, the lock and the speech icons come along for free, and they change
	// as the player looks around. Nothing here knows what any of them mean.
	//
	// sizePixels is how big a square, centred on the layout the game believes
	// it drew in, gets taken. It is a setting rather than a constant because
	// the icons are larger than the plain cross and neither size is written
	// down anywhere this code can read.
	//
	// False when the layer has nothing to give - no capture, no HUD overlay,
	// or the copy failed - and the caller then keeps the drawn cross.
	bool TakeFromHud(void* gameDevice, void* hudSurface, UInt32 hudWidth, UInt32 hudHeight,
	                 UInt32 believedWidth, UInt32 believedHeight, UInt32 sizePixels);

	// Keeps a copy of the crosshair currently in the texture, so third person
	// can show the GAME'S crosshair rather than one drawn here.
	//
	// The picture exists - Oblivion draws it every frame in first person - it
	// simply is not drawn in third person, where the engine leaves the middle
	// of the layer empty. So it is kept from the view that has one and used in
	// the view that does not. What third person shows is then the real
	// crosshair, the player's own replacement texture included, rather than an
	// approximation of it.
	//
	// The caller decides when this is worth doing, and should only call it
	// while NOTHING is under the crosshair: with a target the lifted square
	// holds a context icon - a hand, a lock, a speech bubble - and keeping one
	// of those would freeze the wrong picture into every later frame.
	//
	// Refreshed rather than taken once, so the copy follows the game: the sneak
	// eye replaces the cross while sneaking, and third person then shows the
	// eye too, which is what vanilla does in that one case.
	bool RememberCrosshair(void* gameDevice);

	// Puts the kept copy back into the texture and marks it as something to
	// show. False when nothing has been kept yet - a session that has not been
	// in first person since it started - and the caller then has the drawn
	// cross below as a last resort.
	bool UseRememberedCrosshair(void* gameDevice);

	// Draws a simple reticle of OBVR's own. THIRD PERSON, AND ONLY WHEN THE
	// KEPT COPY IS NOT AVAILABLE.
	//
	// The last resort behind RememberCrosshair, not the first choice: the game's
	// own picture is better in every way that matters, and this is what is left
	// when there has not been one yet.
	//
	// The class comment above says there is no drawn fallback left, and that a
	// hand-drawn near-miss reads as the game getting it wrong rather than as
	// the mod being off. That reasoning is sound and it is why nothing is drawn
	// in first person - but it rests on the game drawing a crosshair to be
	// mistaken for. In third person it draws none at all.
	//
	// That is vanilla behaviour and not something OBVR does: Bethesda's own
	// support page states it plainly - "The crosshair is only visible in
	// Oblivion in First Person mode. It does not appear in 3rd Person mode." -
	// and the mods that add one (Third Person Crosshair, DarNified UI's toggle)
	// exist for that reason. So there is nothing here to be confused with, and
	// lifting cannot help: there is nothing in the layer to lift.
	//
	// Deliberately NOT a copy of Oblivion's cross. Four strokes with a gap in
	// the middle, which reads as a reticle the mod put there.
	//
	// clearFirst false draws over whatever the lift already put in the texture,
	// so a context icon - if the game shows one in third person - is kept and
	// the cross is added to it.
	bool DrawCross(void* gameDevice, bool clearFirst);

	void Destroy();

private:
	bool EnsureTexture(void* gameDevice);
	bool EnsureKeptTexture(void* gameDevice);
	bool EnsureOverlay(vr::OpenVRBackend& backend);
	void Place(vr::OpenVRBackend& backend, float distanceMetres, float widthMetres);

	void* m_texture = nullptr;  // IDirect3DTexture9
	void* m_surface = nullptr;  // IDirect3DSurface9, level 0
	void* m_interop = nullptr;  // ID3D9VkInteropTexture
	BackBufferImage m_image;

	bool m_textureTried = false;

	// Whether the game's own crosshair reached this texture this frame. With
	// no drawn fallback left, a frame that lifted nothing shows nothing.
	bool m_takenFromHud = false;
	bool m_takeReported = false;
	bool m_crossReported = false;

	// The game's own crosshair, kept from first person for third person to use.
	// No interop and no overlay of its own: it is never handed to the
	// compositor, only copied back into the texture that is.
	void* m_kept = nullptr;         // IDirect3DTexture9
	void* m_keptSurface = nullptr;  // IDirect3DSurface9, level 0
	bool m_keptTried = false;
	bool m_haveKept = false;
	bool m_keptReported = false;

	vr::openvr::VROverlayHandle m_overlay = vr::openvr::kOverlayHandleInvalid;
	bool m_overlayTried = false;
	bool m_overlayVisible = false;

	// What the quad was last placed at, so a frame that changes nothing does
	// not talk to the compositor. Once the distance follows what the aim ray
	// hits, this will change most frames - but it will also change by
	// millimetres, and the comparison is what keeps those from becoming calls.
	float m_placedDistance = 0.0f;
	float m_placedWidth = 0.0f;
	bool m_placed = false;

	VulkanContext m_vulkan;
	bool m_vulkanChecked = false;
	bool m_vulkanUsable = false;

	bool m_liveReported = false;
	bool m_failureReported = false;

	InteropBracket m_bracket;
};

}  // namespace obvr::render
