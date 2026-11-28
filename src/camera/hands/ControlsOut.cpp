#include "camera/hands/ControlsOut.h"

#include <cstdio>

#include "camera/FrameLogic.h"
#include "camera/hands/SpellCharge.h"
#include "core/Log.h"
#include "core/MathFns.h"
#include "core/Rotation.h"
#include "game/AnalogWalk.h"
#include "game/EngineInput.h"
#include "game/FlyCam.h"
#include "game/GameSound.h"
#include "game/OptionsMenuVr.h"
#include "game/PlayerTeleport.h"
#include "game/HandControls.h"
#include "game/MeleeHits.h"
#include "game/MenuPause.h"
#include "game/MenuType.h"
#include "render/GameDevice.h"
#include "vr/FlyCamLogic.h"
#include "vr/HandInput.h"
#include "vr/Holster.h"
#include "vr/MenuPlace.h"
#include "test/HandScriptRuntime.h"
#include "vr/MirrorPage.h"
#include "vr/TabletBook.h"
#include "vr/TabletPages.h"
#include "vr/TabletPaper.h"

namespace obvr::camera::hands {

namespace {

// One line a press of the drop button in a menu (2026-11-13: "A links ging
// nicht mehr" in the headset, the harness dropping): which menu was active,
// whether the drop began, whether the game's window was in front (the keys
// then go through the system, behind it through the engine's own input), and
// the tile under the cursor.
void LogDropPress(ControlsOutState& state, vr::DropGate gate) {
	if (gate == vr::DropGate::None || state.dropLinesLeft == 0) {
		return;
	}
	--state.dropLinesLeft;
	char tileName[64] = {};
	game::ActiveTileName(tileName, sizeof(tileName));
	OBVR_LOG("Inventory drop: the left A in the %s menu - %s; the game's window %s; the tile under the cursor \"%s\"",
	         game::MenuIdName(game::ActiveMenuId()),
	         gate == vr::DropGate::Drop ? "Shift and a click on the item" : "nothing: not the inventory",
	         game::GameWindowInFront() ? "in front" : "behind another window", tileName);
}

// Every jump sent and its input, the first several dozen: a flick and a
// diagonal turn told apart (docs/controls-spec.md 2), OBVR_Controls.ini seen.
// And every bound action the frame it fires, with its input, the first
// vr::kBoundFireLines of each (the controls matrix reads them).
void LogJumpAndBoundFires(ControlsOutState& state, bool jump, const vr::HandModeResult& hand) {
	if (jump && !state.jumpWasSent && state.jumpLinesLeft > 0) {
		--state.jumpLinesLeft;
		OBVR_LOG("Hands: jump sent (%s)", hand.jumpBy);
	}
	state.jumpWasSent = jump;
	for (UInt32 i = 0; i < vr::kControlActionCount; ++i) {
		const vr::ControlAction action = static_cast<vr::ControlAction>(i);
		if (vr::StepBoundFireLine(state.boundFireLog, action, hand.firedBy[i])) {
			OBVR_LOG("Buttons: %s from the %s", vr::ControlActionName(action), vr::ControlSourcePhrase(hand.firedBy[i]));
		}
	}
}

bool menuIsUpNow(const vr::HandModeFrame& frame) { return frame.menuMode; }

// The colours of the figure's own page (vr::SetFigurePage): a dark edge and
// the paper, opaque, near the page's own parchment - the paper's colour only
// until the game's paper is loaded (vr/TabletPaper.h).
constexpr UInt32 kFigurePageEdge = 0xFF4A3A28u;
constexpr UInt32 kFigurePagePaper = 0xFFE4D5B1u;

void Piece(render::TabletQuadPiece& piece, const vr::TabletCorners& corners, bool textured, UInt32 colour) {
	piece.valid = corners.valid;
	piece.textured = textured;
	piece.colour = colour;
	for (int i = 0; i < 4; ++i) {
		piece.corners[i] = corners.at[i];
		piece.u[i] = corners.u[i];
		piece.v[i] = corners.v[i];
	}
}

constexpr UInt32 kTabChosen = 0xFFE4D5B1u;  // the paper's colour: the tab in front
constexpr UInt32 kTabBehind = 0xFFA8946Cu;  // darker: the tab behind
// On the game's paper (vr/TabletPaper.h): the tab in front as the paper is,
// the one behind darkened by about what kTabBehind is of kTabChosen.
constexpr UInt32 kTabChosenTint = 0xFFFFFFFFu;
constexpr UInt32 kTabBehindTint = 0xFFBCB09Cu;

// A piece on the game's paper, `source` in the paper's texels; `colour` while
// the paper is not loaded.
void PaperPiece(render::TabletQuadPiece& piece, const vr::TabletPlace& place, const vr::TabletFrame& t,
                const vr::UiRect& rect, const vr::UiRect& source, float share, bool upright, UInt32 colour,
                UInt32 tint) {
	Piece(piece,
	      vr::RectOnTablet(place, t.crop, rect, source, vr::kTabletPaperSide, vr::kTabletPaperSide, share, upright),
	      false, colour);
	piece.paper = true;
	piece.tint = tint;
}

// A small picture of `source` in the middle of a tab, its height 0.7 of the tab's.
vr::UiRect TabPicture(const vr::UiRect& tab, const vr::UiRect& source) {
	vr::UiRect r;
	const float height = 0.7f * (tab.bottom - tab.top);
	const float sourceHeight = source.bottom - source.top;
	if (!tab.valid || !source.valid || !(sourceHeight > 0.0f)) {
		return r;
	}
	const float width = height * (source.right - source.left) / sourceHeight;
	const float middle = 0.5f * (tab.left + tab.right);
	r.left = middle - 0.5f * width;
	r.right = middle + 0.5f * width;
	r.top = 0.5f * (tab.top + tab.bottom) - 0.5f * height;
	r.bottom = r.top + height;
	r.valid = true;
	return r;
}

// What the strip's pieces are laid out on: the tablet's place and face, the
// layer's texture size, how much of it is unrolled, and its way up.
struct StripDraw {
	const vr::TabletPlace& place;
	const vr::TabletFrame& t;
	float textureWidth;
	float textureHeight;
	float share;
	bool upright;
};

constexpr UInt32 kMirrorFrameColour = kFigurePageEdge;  // the frame: the figure page's dark edge
constexpr UInt32 kMirrorGlassColour = 0xFF0B1115u;      // dark glass, until a reflection stands in it
constexpr UInt32 kMirrorIconRimColour = 0xFF8A6A3Au;    // the tab's hand mirror: a bronze rim
constexpr UInt32 kMirrorIconGlassColour = 0xFFA9BFC8u;  // and pale glass

// A plain piece over `rect` of the face.
void PlainPiece(render::TabletQuadPiece& piece, const StripDraw& d, const vr::UiRect& rect, UInt32 colour) {
	const vr::UiRect none;
	Piece(piece, vr::RectOnTablet(d.place, d.t.crop, rect, none, d.textureWidth, d.textureHeight, d.share, d.upright),
	      false, colour);
}

// The Mirror tab (vr::MirrorIconIn): the tab on the game's paper, a hand
// mirror drawn on it - a rim and its glass in plain colour.
void MirrorTabPieces(render::TabletQuadFrame& quad, const StripDraw& d, const vr::UiRect& tab, bool chosen) {
	const vr::MirrorIconRects icon = vr::MirrorIconIn(tab);
	PaperPiece(quad.pieces[render::kTabletMirrorTabPieces], d.place, d.t, tab, vr::ParchmentSource(tab), d.share, d.upright,
	           chosen ? kTabChosen : kTabBehind, chosen ? kTabChosenTint : kTabBehindTint);
	PlainPiece(quad.pieces[render::kTabletMirrorTabPieces + 1], d, icon.rim, kMirrorIconRimColour);
	PlainPiece(quad.pieces[render::kTabletMirrorTabPieces + 2], d, icon.glass, kMirrorIconGlassColour);
}

// The mirror page over the page's picture (vr::MirrorPageOf): the frame, the
// glass in it - plain dark colour here; stage 2's reflection replaces it
// through render::TabletQuad::SetMirrorGlass - and the dust over the glass,
// blended, unless it is off.
void MirrorPagePieces(render::TabletQuadFrame& quad, const StripDraw& d, bool dust) {
	const vr::MirrorPageRects page = vr::MirrorPageOf(d.t.paper);
	render::TabletQuadPiece* pieces = quad.pieces + render::kTabletMirrorPagePieces;
	PlainPiece(pieces[0], d, page.frame, kMirrorFrameColour);
	PlainPiece(pieces[1], d, page.glass, kMirrorGlassColour);
	pieces[1].glass = true;
	if (dust) {
		PlainPiece(pieces[2], d, page.glass, 0xFFFFFFFFu);
		pieces[2].dust = true;
		const float u[4] = {0.0f, 1.0f, 0.0f, 1.0f};
		const float v[4] = {0.0f, 0.0f, 1.0f, 1.0f};
		for (int i = 0; i < 4; ++i) {
			pieces[2].u[i] = u[i];
			pieces[2].v[i] = v[i];
		}
	}
	quad.mirrorUp = pieces[1].valid;
}

// The tab strip (vr::SetTabs): each tab on the game's paper, the one behind
// darker, the page's and the figure's with a small picture of the page and of
// the figure, the mirror's with a small glass; then what the tab chosen shows:
// the figure over the page's rectangle on an empty page of the game's paper
// (its frame and bar, parchment down to its lower edge), or the mirror page.
void TabPieces(render::TabletQuadFrame& quad, const StripDraw& d, vr::TabletTab shown, bool dust) {
	const vr::TabletFrame& t = d.t;
	const vr::UiRect* sources[2] = {&t.paper, &t.figure};
	for (UInt32 i = 0; i < t.tabCount; ++i) {
		const bool chosen = t.tabKind[i] == shown;
		if (t.tabKind[i] == vr::TabletTab::Mirror) {
			MirrorTabPieces(quad, d, t.tabs[i], chosen);
			continue;
		}
		const UInt32 k = t.tabKind[i] == vr::TabletTab::Figure ? 1u : 0u;
		PaperPiece(quad.pieces[1 + 2 * k], d.place, t, t.tabs[i], vr::ParchmentSource(t.tabs[i]), d.share, d.upright,
		           chosen ? kTabChosen : kTabBehind, chosen ? kTabChosenTint : kTabBehindTint);
		Piece(quad.pieces[2 + 2 * k],
		      vr::RectOnTablet(d.place, t.crop, TabPicture(t.tabs[i], *sources[k]), *sources[k], d.textureWidth,
		                       d.textureHeight, d.share, d.upright),
		      true, 0);
	}
	if (shown == vr::TabletTab::Figure) {
		vr::PaperBand bands[2];
		vr::FigurePaperBands(t.paper, bands);
		for (int i = 0; i < 2; ++i) {
			PaperPiece(quad.pieces[5 + i], d.place, t, bands[i].rect, bands[i].source, d.share, d.upright, kFigurePagePaper,
			           kTabChosenTint);
		}
		Piece(quad.pieces[7],
		      vr::RectOnTablet(d.place, t.crop, t.figureFit, t.figure, d.textureWidth, d.textureHeight, d.share, d.upright),
		      true, 0);
	}
	if (shown == vr::TabletTab::Mirror) {
		MirrorPagePieces(quad, d, dust);
	}
}

// The tablet as drawn into the eyes: the page and its popups where the game
// draws them, and with the figure on its own page that page's edge, its paper
// and the figure - all rising from the lower edge together.
render::TabletQuadFrame TabletPieces(const vr::HandModeResult& hand, const vr::TabletFrame& t, float textureWidth,
                                     float textureHeight, float share, bool mirrorDust) {
	const bool upright = hand.tabletUpright;
	render::TabletQuadFrame quad;
	vr::TabletPlace place;
	place.valid = true;
	place.transform = hand.menuTransform;
	place.width = hand.tabletWidth;
	place.height = hand.tabletHeight;
	// The page, and each popup as its own picture (vr::LayerPictures): drawn
	// as the one rectangle round them, the face showed what the game draws
	// beside them - the inventory's figure, cut off.
	vr::UiRect pictures[1 + vr::kTabletPopupsMost];
	const UInt32 pictureCount = vr::LayerPictures(t, pictures, 1 + vr::kTabletPopupsMost);
	for (UInt32 i = 0; i < pictureCount; ++i) {
		Piece(quad.pieces[i == 0 ? 0 : 9 + i],
		      vr::RectOnTablet(place, t.crop, pictures[i], pictures[i], textureWidth, textureHeight, share, upright), true,
		      0);
	}
	if (t.figurePage) {
		const float edge = 0.006f * (t.paper.right - t.paper.left);
		vr::UiRect border = t.backing;
		border.top -= edge;
		border.bottom += edge;
		border.right += edge;
		const vr::UiRect none;
		Piece(quad.pieces[1], vr::RectOnTablet(place, t.crop, border, none, textureWidth, textureHeight, share, upright), false,
		      kFigurePageEdge);
		PaperPiece(quad.pieces[2], place, t, t.backing, vr::ParchmentSource(t.backing), share, upright, kFigurePagePaper,
		           kTabChosenTint);
		Piece(quad.pieces[3],
		      vr::RectOnTablet(place, t.crop, t.figureAt, t.figure, textureWidth, textureHeight, share, upright), true, 0);
	}
	if (t.tabCount > 0) {
		TabPieces(quad, StripDraw{place, t, textureWidth, textureHeight, share, upright}, hand.tabletTab, mirrorDust);
	}
	// The rolls at the open part's edges while it unrolls.
	const float pageWidth = t.paper.right - t.paper.left;
	const vr::UiRect none;
	for (int side = 0; side < 2; ++side) {
		Piece(quad.pieces[8 + side],
		      vr::RectOnTablet(place, t.crop, vr::RollStrip(t.crop, share, side == 1, pageWidth, upright), none, textureWidth,
		                       textureHeight, 1.0f),
		      false, kFigurePageEdge);
	}
	// The HUD's texts beside the menus, above the page at its scale once it is
	// open (vr/TabletTexts.h) - out past the face, so no tap reaches them.
	if (t.textsAt.valid && share >= 1.0f) {
		Piece(quad.pieces[render::kTabletTextsPiece],
		      vr::RectOnTablet(place, t.crop, t.textsAt, t.texts, textureWidth, textureHeight, 1.0f), true, 0);
	}
	quad.valid = quad.pieces[0].valid;
	return quad;
}

// Both grips closed with the hands together and the scroll not begun: why,
// with the measure that refused it (vr/TabletScroll.h, ScrollRefusal) - a
// gesture that does nothing and says nothing cannot be learned or tuned.
void LogScrollRefusal(ControlsOutState& state, const vr::HandModeResult& hand) {
	const auto why = static_cast<vr::TabletScrollRefusal>(hand.tabletRefusal);
	if (why == vr::TabletScrollRefusal::None || state.scrollRefusalLinesLeft == 0) {
		return;
	}
	--state.scrollRefusalLinesLeft;
	const vr::TabletScrollLimits l;
	const double by = static_cast<double>(hand.tabletRefusedBy);
	char reason[160];
	switch (why) {
	case vr::TabletScrollRefusal::NotAllowed:
		std::snprintf(reason, sizeof(reason), "the tablet is off, the player not in the world, or OBVR's own menu up");
		break;
	case vr::TabletScrollRefusal::HandsHold:
		std::snprintf(reason, sizeof(reason), "a hand holds something (a weapon out, a torch, a thing held, a two-hand grip)");
		break;
	case vr::TabletScrollRefusal::Apart:
		std::snprintf(reason, sizeof(reason), "the controllers %.2f m apart (at most %.2f)", by,
		              static_cast<double>(l.togetherMetres));
		break;
	case vr::TabletScrollRefusal::OutOfRing:
		std::snprintf(reason, sizeof(reason), "the hands' middle %.2f m past the inventory ring (its reach and %.2f)", by,
		              static_cast<double>(l.ringMarginMetres));
		break;
	case vr::TabletScrollRefusal::Late:
		std::snprintf(reason, sizeof(reason), "the second grip %.2f s after the first (at most %.2f)", by,
		              static_cast<double>(l.pairSeconds));
		break;
	default:
		std::snprintf(reason, sizeof(reason), "the hands not tracked");
		break;
	}
	OBVR_LOG("Tablet: both grips closed with the hands together, the scroll not begun - %s", reason);
}

// How the page sits in the hand (vr::HeldPlace), said when the setting
// changes while it is up - Nadi tunes it in the game - and at a hand-script
// mark: the face's way in the holding controller's frame and how far it
// looks off the eyes (vr::FaceOffEyesDegrees).
void LogHeldPlace(ControlsOutState& state, const vr::HeldPlace& place, const vr::HandModeResult& hand,
                  const vr::HandModeFrame& frame) {
	const auto same = [](const vr::HeldPlace& a, const vr::HeldPlace& b) {
		return a.turnDegrees == b.turnDegrees && a.leanDegrees == b.leanDegrees && a.rollDegrees == b.rollDegrees &&
		       a.acrossCm == b.acrossCm && a.upCm == b.upCm && a.outCm == b.outCm;
	};
	const bool mark = test::HandScriptMarkedThisFrame();
	const bool changed = !state.heldSaidOnce || !same(place, state.heldSaid);
	if (!mark && !(changed && state.heldLinesLeft > 0)) {
		return;
	}
	if (changed && state.heldLinesLeft > 0) {
		--state.heldLinesLeft;
	}
	state.heldSaidOnce = true;
	state.heldSaid = place;
	const vr::openvr::HmdMatrix34& m = hand.menuTransform;
	const vr::HandPose& holder = hand.tabletHolderRight ? frame.right : frame.left;
	OBVR_LOG("Tablet: held turned %.0f, leant %.0f, tilted %.0f degrees, moved %.0f %.0f %.0f cm (across, up, out) - "
	         "its face %.2f %.2f %.2f in the %s controller's frame, %.0f degrees off the eyes%s",
	         static_cast<double>(place.turnDegrees), static_cast<double>(place.leanDegrees),
	         static_cast<double>(place.rollDegrees), static_cast<double>(place.acrossCm), static_cast<double>(place.upCm),
	         static_cast<double>(place.outCm), static_cast<double>(m.m[0][2]), static_cast<double>(m.m[1][2]),
	         static_cast<double>(m.m[2][2]),
	         vr::PhysicalLeftOf(!hand.tabletHolderRight, frame.rolesSwapped) ? "left" : "right",
	         static_cast<double>(vr::FaceOffEyesDegrees(m, holder, frame.headPosition)), mark ? " (at the mark)" : "");
}

void PlaceLayerOverlay(ControlsOutState& state, const Config& config, vr::OpenVRBackend& backend,
                       const vr::HandModeResult& hand, const vr::HandModeFrame& frame, const ControlsOutLinks& links,
                       bool drawnBoth, bool closing, bool lastFull);

// The game's menus and the HUD on the hands: the tablet in the off hand -
// drawn into the eyes (render/TabletQuad.h) or, when that is off or did not
// reach both eyes last frame, the overlay on the holding controller showing
// the menus' cut - or a wrist, or neither.
void PlaceMenusOnHands(ControlsOutState& state, const Config& config, vr::OpenVRBackend& backend,
                       const vr::HandModeResult& hand, const vr::HandModeFrame& frame, const ControlsOutLinks& links) {
	const bool drawnBoth = links.headsetRenderer.TakeTabletDrawnBoth();
	state.tabletSeconds = hand.menuOnTablet ? state.tabletSeconds + links.deltaSeconds : 0.0f;
	render::TabletQuadFrame quad;
	const float textureWidth = static_cast<float>(links.hudLayer.CaptureWidth());
	const float textureHeight = static_cast<float>(links.hudLayer.CaptureHeight());
	// Last frame's tablet set whole (vr::TabletOnOverlay), and this frame's share.
	const bool lastFull = state.tabletQuadFull;
	state.tabletDrawnShare = 0.0f;
	if (hand.menuOnTablet && config.hands.tablet.inScene) {
		// Unrolled between the hands by the scroll's amount, or in the hand by
		// itself over the first moments (vr/TabletScroll.h).
		if (hand.tabletShare >= 0.0f) {
			state.tabletSeconds = vr::kTabletUnrollSeconds;
		}
		// Rolled up with its close sent: nothing, not the page open again (vr/TabletOverlayGate.h).
		const float share =
			vr::TabletDrawnShare(hand.tabletRolledUp, hand.tabletShare, vr::TabletUnrollShare(state.tabletSeconds));
		quad = TabletPieces(hand, frame.tablet, textureWidth, textureHeight, share, config.hands.tablet.mirrorDust);
		quad.texture = links.hudLayer.CaptureTexture();
		state.tabletDrawnShare = share;
	}
	state.tabletQuadFull = quad.valid && state.tabletDrawnShare >= 1.0f;
	links.headsetRenderer.SetTablet(quad);
	if (config.hands.tablet.enabled && config.hands.tablet.inScene && game::TopVisibleMenu() == game::kMenuIdMain) {
		links.headsetRenderer.PrepareTablet(render::GetGameDevice());  // the game's paper, before the first open
	}
	if (hand.menuOnTablet && hand.tabletTab != state.tabletShowedTab) {
		OBVR_LOG("Tablet: the %s tab chosen", vr::TabletTabName(hand.tabletTab));
	}
	state.tabletShowedTab = hand.menuOnTablet ? hand.tabletTab : vr::TabletTab::Page;
	// The scroll (vr/TabletScroll.h), said as it changes.
	LogScrollRefusal(state, hand);
	if (hand.tabletMenuKey) {
		OBVR_LOG("Tablet: the scroll sends the menu key (%s)", menuIsUpNow(frame) ? "the menus close" : "the menus open");
	}
	const bool between = hand.tabletShare >= 0.0f;
	if (between != state.tabletWasBetween) {
		OBVR_LOG("Tablet: %s", between ? "unrolling between the hands" : "no longer between the hands");
	}
	state.tabletWasBetween = between;
	// The hand that kept its grip as the other let go holds it (vr/TabletHolder.h):
	// said when that is the weapon hand, with its controller.
	if (hand.tabletHolderRight != state.tabletWasRight) {
		OBVR_LOG("Tablet: %s",
		         !hand.tabletHolderRight                         ? "not in the weapon hand"
		         : vr::PhysicalLeftOf(false, links.rolesSwapped) ? "kept by the weapon hand - on the left controller"
		                                                         : "kept by the weapon hand - on the right controller");
	}
	state.tabletWasRight = hand.tabletHolderRight;
	if (hand.menuOnTablet != state.tabletWasUp) {
		state.tabletWasUp = hand.menuOnTablet;
		// Which controller holds it, said with the open (vr/TabletHolder.h).
		const bool onLeft = vr::PhysicalLeftOf(!hand.tabletHolderRight, links.rolesSwapped);
		OBVR_LOG("Tablet: %s%s", hand.menuOnTablet ? (config.hands.tablet.inScene ? "in the off hand, drawn into the eyes"
		                                                                         : "in the off hand, as an overlay")
		                                           : "put down - the menus as before",
		         !hand.menuOnTablet ? "" : (onLeft ? " - on the left controller" : " - on the right controller"));
	}
	// The frames its menus close on are the tablet's still (vr/TabletClose.h).
	const UInt32 fadeFrames = state.tabletClose.frames;
	const bool closing = vr::StepTabletClose(state.tabletClose, hand.menuOnTablet, frame.tablet.menusGone);
	if (!closing && fadeFrames > 0) {
		OBVR_LOG("Tablet: %s - the menus' fade kept out of the room and the eyes (%u frame(s))",
		         hand.menuOnTablet ? "the pages back" : "closed", fadeFrames);
	}
	if (hand.menuOnTablet) {
		LogHeldPlace(state, config.hands.tablet.held, hand, frame);
	}
	PlaceLayerOverlay(state, config, backend, hand, frame, links, drawnBoth, closing, lastFull);
}

// The layer's overlay this frame: the tablet's on the holding controller
// (hidden while the eyes draw it), kept off through its menus' close, a
// wrist, or the room's panel - primed for a frame as it comes back after the
// tablet (vr::StepOverlayReturn).
void PlaceLayerOverlay(ControlsOutState& state, const Config& config, vr::OpenVRBackend& backend,
                       const vr::HandModeResult& hand, const vr::HandModeFrame& frame, const ControlsOutLinks& links,
                       bool drawnBoth, bool closing, bool lastFull) {
	const float textureWidth = static_cast<float>(links.hudLayer.CaptureWidth());
	const float textureHeight = static_cast<float>(links.hudLayer.CaptureHeight());
	const bool onTablet = hand.menuOnTablet && textureWidth > 0.0f && textureHeight > 0.0f;
	// On the controller only for a whole tablet the eyes missed; nothing of the
	// gesture's menus before the tablet has them (vr/TabletOverlayGate.h).
	const bool tabletHidden =
		!vr::TabletOnOverlay(config.hands.tablet.inScene, state.tabletDrawnShare, drawnBoth, lastFull);
	const bool gestureHolds =
		vr::GestureHoldsMenus(frame.menuMode, hand.menuOnTablet, hand.tabletShare >= 0.0f, hand.tabletRolledUp);
	if (vr::StepOverlayReturn(state.overlayReturn, closing || gestureHolds || (onTablet && tabletHidden))) {
		links.hudLayer.PrimeNextSubmit();
	}
	if (onTablet) {
		const vr::openvr::VRTextureBounds bounds{frame.tablet.crop.left / textureWidth, frame.tablet.crop.top / textureHeight,
		                                         frame.tablet.crop.right / textureWidth,
		                                         frame.tablet.crop.bottom / textureHeight};
		links.hudLayer.SetTabletPlacement(backend.HandDeviceIndex(vr::HandDeviceForRole(false, links.rolesSwapped)),
		                                  hand.menuTransform, hand.tabletWidth, bounds, tabletHidden);
	} else if (closing || gestureHolds) {
		links.hudLayer.HideTablet();
	} else if (hand.menuOnWrist) {
		links.hudLayer.SetWristPlacement(backend.HandDeviceIndex(vr::HandDeviceForRole(hand.menuWristRight, links.rolesSwapped)),
		                             hand.menuTransform, config.hands.wristMenuWidth);
	} else if (hand.hudOnRightWrist) {
		links.hudLayer.SetWristPlacement(backend.HandDeviceIndex(vr::HandDeviceForRole(true, links.rolesSwapped)), hand.hudTransform,
		                             config.hands.wristHudWidth);
	} else {
		links.hudLayer.ClearWristPlacement();
	}
}

// The right stick's side under the tablet: the page beside the one shown, by
// its key (vr/TabletPages.h), held for kPageKeyFrames frames so the menu
// reads it once.
constexpr UInt32 kPageKeyFrames = 3;

// What the finger did on the tablet's glass, for the log (the harness's tablet-touch.txt reads it).
void LogTabletTouch(ControlsOutState& state, const vr::HandModeResult& hand) {
	const vr::TabletTouchEvent event = hand.tabletTouch.event;
	// Counted from the drag's start, which seldom gives a notch on its own
	// frame (it starts at 1.2 cm, a notch is 2) - until the line is said.
	state.touchScrolling = state.touchScrolling || event == vr::TabletTouchEvent::ScrollStart;
	state.touchNotches += state.touchScrolling ? hand.menuScroll : 0;
	const bool moved = hand.menuScroll != 0 || event == vr::TabletTouchEvent::ScrollStart;
	state.touchQuietFrames = moved ? 0 : state.touchQuietFrames + 1;
	state.touchScrolling = state.touchScrolling && (state.touchNotches != 0 || state.touchQuietFrames < 30);
	if (hand.pokeHover && test::HandScriptMarkedThisFrame()) {
		char tile[64] = {};
		game::ActiveTileName(tile, sizeof(tile));
		OBVR_LOG("Tablet: at the mark the finger is over \"%s\" in the %s menu", tile,
		         game::MenuIdName(game::ActiveMenuId()));
	}
	if (state.touchLinesLeft == 0) {
		return;
	}
	if (state.touchNotches != 0 && state.touchQuietFrames >= 30) {
		--state.touchLinesLeft;
		OBVR_LOG("Tablet: the finger's scroll moved the list %d notch(es) %s", state.touchNotches,
		         state.touchNotches > 0 ? "up" : "down");
		state.touchNotches = 0;
		state.touchScrolling = false;
	}
	const char* const what = event == vr::TabletTouchEvent::Tap ? "tapped - the click on the release, where it went down"
	                         : event == vr::TabletTouchEvent::ScrollStart ? "drawn up or down - the list scrolls, no click"
	                         : event == vr::TabletTouchEvent::PageTurn    ? "swiped sideways - the page turns, no click"
	                         : event == vr::TabletTouchEvent::HoldStart   ? "held still - the button down until it leaves"
	                         : event == vr::TabletTouchEvent::Cancelled   ? "gone from the glass - nothing clicked"
	                         : event == vr::TabletTouchEvent::Moved       ? "moved and let go - nothing clicked"
	                                                                     : nullptr;
	// With the touch's measures - the numbers the headset tunes by - and, as a
	// touch ends, how.
	const vr::TabletContactEnd end = hand.tabletTouch.end;
	const char* const how = end == vr::TabletContactEnd::Lifted       ? "lifted off in front of the glass"
	                        : end == vr::TabletContactEnd::PulledBack ? "pulled back behind the glass"
	                        : end == vr::TabletContactEnd::OffEdge    ? "off the face's edge"
	                        : end == vr::TabletContactEnd::TooDeep    ? "too far through the glass"
	                        : end == vr::TabletContactEnd::FingerIn   ? "the index curled or the trigger pulled"
	                                                                  : nullptr;
	if (state.touchLinesLeft > 0 && (what != nullptr || how != nullptr)) {
		--state.touchLinesLeft;
		OBVR_LOG("Tablet: the finger %s%s%s (%.2f s on the glass, moved %.1f cm, at its deepest %.1f cm %s it)",
		         what != nullptr ? what : "let go", how != nullptr ? " - " : "", how != nullptr ? how : "",
		         static_cast<double>(hand.tabletTouch.seconds), static_cast<double>(hand.tabletTouch.travelMetres * 100.0f),
		         static_cast<double>(hand.tabletTouch.deepestMetres < 0.0f ? -hand.tabletTouch.deepestMetres * 100.0f
		                                                                  : hand.tabletTouch.deepestMetres * 100.0f),
		         hand.tabletTouch.deepestMetres < 0.0f ? "through" : "in front of");
	}
}

// A book's or a letter's turn (vr/TabletBook.h): a wheel notch on a book,
// the menu's own turn; nothing on a letter. Said a few times.
void TurnBook(ControlsOutState& state, const vr::HandModeResult& hand, vr::TabletLook look) {
	const vr::TabletTurn turn = vr::TabletTurnFor(look, hand.tabletPageStep);
	if (turn.wheel != 0) {
		game::ScrollMouseWheel(turn.wheel);
	}
	if (state.bookTurnLinesLeft > 0) {
		--state.bookTurnLinesLeft;
		OBVR_LOG("Tablet: %s turns the %s %s%s", hand.tabletTouch.event == vr::TabletTouchEvent::PageTurn ? "the finger" : "the right stick",
		         look == vr::TabletLook::Book ? "book's page" : "letter", hand.tabletPageStep > 0 ? "on" : "back",
		         turn.wheel != 0 ? (turn.wheel > 0 ? " - a wheel notch up sent" : " - a wheel notch down sent")
		                         : " - a letter has one page, nothing sent");
	}
}

void StepTabletPageKey(ControlsOutState& state, const vr::HandModeResult& hand, const vr::TabletFrame& tablet) {
	LogTabletTouch(state, hand);
	if (state.pageKeyFrames > 0 && --state.pageKeyFrames == 0) {
		game::TapKey(state.pageKey, false, false);
		state.pageKey = 0;
	}
	if (hand.tabletPageStep == 0 || state.pageKey != 0) {
		return;
	}
	const vr::TabletLook look = vr::TabletLookOf(tablet);
	if (look != vr::TabletLook::Pages) {
		TurnBook(state, hand, look);
		return;
	}
	const UInt32 shown = tablet.page;
	const UInt32 key = vr::TabletPageKeyAfter(shown, hand.tabletPageStep);
	OBVR_LOG("Tablet: %s turns the page %s from %s%s",
	         hand.tabletTouch.event == vr::TabletTouchEvent::PageTurn ? "the finger" : "the right stick",
	         hand.tabletPageStep > 0 ? "right" : "left",
	         game::MenuIdName(shown), key != 0 ? "" : " - no page shown, nothing sent");
	if (key != 0) {
		game::TapKey(key, false, true);
		state.pageKey = key;
		state.pageKeyFrames = kPageKeyFrames;
	}
}

// A walk with the tablet up, said as it ends: how far the feet went and how
// many clicks went to the tablet's menus on the way - the walk and the
// clicks at once, none of them a blow (game/MenuPause.h).
void LogTabletWalk(ControlsOutState& state, const vr::HandModeResult& hand, const vr::HandModeFrame& frame,
                   bool clickEdge) {
	const bool walking = frame.walkUnderTablet && hand.walkingUnderMenu;
	NiPoint3 feet{0.0f, 0.0f, 0.0f};
	if (walking && !state.tabletWalking) {
		state.tabletWalkFeetKnown = game::ReadPlayerFeet(state.tabletWalkFrom);
		state.tabletWalkClicks = 0;
	}
	state.tabletWalkClicks += walking && clickEdge ? 1u : 0u;
	if (!walking && state.tabletWalking && state.tabletWalkLinesLeft > 0) {
		--state.tabletWalkLinesLeft;
		const bool known = state.tabletWalkFeetKnown && game::ReadPlayerFeet(feet);
		const NiPoint3 d = feet - state.tabletWalkFrom;
		OBVR_LOG("Tablet: walked %.0f units with it open, %u click(s) sent to its menus on the way%s",
		         known ? static_cast<double>(math::Sqrt(d.x * d.x + d.y * d.y)) : -1.0, state.tabletWalkClicks,
		         known ? "" : " (the feet unread)");
	}
	state.tabletWalking = walking;
	StepTabletPageKey(state, hand, frame.tablet);
}


}  // namespace

void ResetControlsOut(ControlsOutState& state, const Config& config) {
	if (state.controlsHeld) {
		game::ReleaseHandControls(config.handKeys);
		state.controlsHeld = false;
	}
	state.tabletClose = vr::TabletCloseState{};
}

void StepControlsOut(ControlsOutState& state, const HandStep& step, const HandShared& shared,
                     const vr::HandModeFrame& frame, const ControlsOutLinks& links) {
	const Config& config = step.config;
	vr::OpenVRBackend& backend = step.backend;
	const bool active = step.active;
	const bool menuIsUp = step.menuIsUp;
	const vr::HandModeResult& hand = shared.hand;
	const HandStep& handStep = step;
	const HandShared& handShared = shared;
	// The laser is the pointer: the game's own cursor sprite goes for as long
	// as the hand-tracked mode runs - it showed on the HUD in game as well -
	// and, with only the menus on the controllers, while a laser points at a
	// game menu. It comes back when neither holds.
	game::SetMenuCursorHidden(
		(active || (menuIsUp && hand.laserVisible)) && !frame.settingsMenuOpen);
	
	if (menuIsUp && !state.quadLaserMenuWasUp) {
		state.quadLaserLinesLeft = 4;
	}
	state.quadLaserMenuWasUp = menuIsUp;
	if (menuIsUp && !frame.flat.valid && state.quadLaserLinesLeft > 0 &&
	    (hand.laserVisible || hand.menuOnWrist)) {
		// Once on the first frame, then only while the trigger is pulled, so
		// the lines that are left show what a click saw.
		const bool pulled = frame.right.trigger > 0.5f || frame.left.trigger > 0.5f;
		if (state.quadLaserLinesLeft == 4 || pulled) {
			--state.quadLaserLinesLeft;
			OBVR_LOG("Hands: menu laser - %s quad %s (%.0fx%.0f layer px), cursor %s %.0f,%.0f, "
			         "%s hand points, hit %s %.0f,%.0f, step %d,%d, triggers %.2f/%.2f, click %d, "
			         "controls %s",
			         hand.menuOnWrist ? "wrist" : "big", frame.menuQuad.valid || hand.menuOnWrist
			                                                   ? "valid" : "MISSING",
			         static_cast<double>(frame.layerPixelsWidth),
			         static_cast<double>(frame.layerPixelsHeight),
			         frame.cursorValid ? "at" : "UNREADABLE", static_cast<double>(frame.cursorX),
			         static_cast<double>(frame.cursorY), hand.laserRight ? "right" : "left",
			         hand.laserHit ? "at" : "NONE", static_cast<double>(hand.laserPixelX),
			         static_cast<double>(hand.laserPixelY), hand.cursorDx, hand.cursorDy,
			         static_cast<double>(frame.right.trigger), static_cast<double>(frame.left.trigger),
			         hand.controls.menuClick ? 1 : 0,
			         hand.controlsActive ? "active" : "INACTIVE - nothing is sent");
		}
	}
	
	if (hand.controlsActive) {
		// Snap turning: the stick's turn becomes whole steps written into the
		// player's heading, and the continuous mouse turn is dropped. Written
		// here, at Present, so the next camera pass builds the view on it.
		vr::HandControlsWanted controls = WithSpellCharge(hand.controls, links.deltaSeconds, menuIsUp, config.hands.spellAim.charge);
		// The engine's fly camera (game/FlyCam.h, vr/FlyCamLogic.h): the
		// sticks fly and turn it, the snap turn's steps turning its yaw, not
		// the player's.
		game::FlyCamPose fly;
		const bool flying = !menuIsUp && game::ReadFlyCam(fly) && fly.on;
		const SnapTurnStep snap =
			StepSnapTurn(state.snapTurn, flying ? hand.flyTurnX : controls.turn, config.look, links.deltaSeconds,
			             !menuIsUp && game::PlayerInWorld());
		if (snap.ownsStick) {
			controls.turn = 0.0f;
		}
		if (snap.fired && config.look.snapTurnVignette) {
			shared.vignette.Trigger();
		}
		if (flying) {
			vr::FlyCamInput in;
			in.moveX = hand.flyMoveX;
			in.moveY = hand.flyMoveY;
			in.turnX = hand.flyTurnX;
			in.liftY = hand.flyLiftY;
			Heading head{};
			if (HeadingOf(links.cameraRotation, head)) {
				in.headYaw = math::Atan2(head.sine, head.cosine);
			}
			in.snapYaw = snap.yawRadians;
			in.snapOwnsStick = snap.ownsStick;
			in.dtSeconds = links.deltaSeconds;
			const vr::FlyCamStep flyStep = vr::StepFlyCam(in, fly.yaw, config.hands.flyCam);
			if (flyStep.move.x != 0.0f || flyStep.move.y != 0.0f || flyStep.move.z != 0.0f || flyStep.yaw != 0.0f) {
				const NiPoint3 at{fly.at.x + flyStep.move.x, fly.at.y + flyStep.move.y, fly.at.z + flyStep.move.z};
				const bool wrote = game::WriteFlyCam(at, math::WrapAngle(fly.yaw + flyStep.yaw));
				if (state.flyLines > 0) {
					--state.flyLines;
					OBVR_LOG("FlyCam: the sticks %s - by %.1f %.1f %.1f, the yaw by %.1f degrees",
					         wrote ? "fly it" : "could not write it", static_cast<double>(flyStep.move.x),
					         static_cast<double>(flyStep.move.y), static_cast<double>(flyStep.move.z),
					         static_cast<double>(flyStep.yaw) * 57.29578);
				}
			}
		} else if (snap.yawRadians != 0.0f) {
			game::PlayerRotation rotation{};
			// rotZ grows clockwise and PlayerYawForGaze subtracts its step,
			// so a turn to the right goes in negated.
			const bool turned = game::ReadPlayerRotation(rotation) &&
			                    game::WritePlayerYaw(PlayerYawForGaze(rotation.yaw, -snap.yawRadians));
			if (snap.fired && !state.snapTurnReported) {
				state.snapTurnReported = true;
				OBVR_LOG("Look: snap turn %.0f degrees %s", static_cast<double>(config.look.snapTurnAngle),
				         turned ? "written into the player's heading"
				                : "could not be written - the player could not be reached");
			}
		}
	
		// The ready weapon tap, the first several times, with where the
		// weapon stood: "the button does nothing" was reported on
		// 2026-09-25, and this says whether the press left OBVR at all.
		if (controls.readyWeapon && !state.readyWasWanted && state.readyLinesLeft > 0) {
			--state.readyLinesLeft;
			const game::WeaponState weapon = game::ReadPlayerWeaponState();
			OBVR_LOG("Hands: ready weapon sent (the weapon was %s)",
			         weapon == game::WeaponState::Drawn      ? "drawn"
			         : weapon == game::WeaponState::Sheathed ? "sheathed"
			                                                 : "unknown");
		}
		// How long the draw or sheathe took, from the key to the game showing
		// it with no animation left: the length of the animation, which
		// WeaponDrawSpeed shortens.
		if (controls.readyWeapon && !state.readyWasWanted) {
			state.readySeconds = 0.0f;
		} else {
			state.readySeconds += links.deltaSeconds;
		}
		state.readyWasWanted = controls.readyWeapon;
		if ((hand.ready.reached || hand.ready.gaveUp) && state.readyEndLinesLeft > 0) {
			--state.readyEndLinesLeft;
			// With the player's action code: a give-up while blocking or in an
			// equip animation throughout never sent the key at all.
			// And the weapon's type: two-handers were still seen settling for
			// about a second after the draw (2026-09-27).
			SInt32 weaponType = -1;
			game::EquippedWeaponForm(&weaponType);
			OBVR_LOG("Hands: ready weapon %s after %.2f s (player action %d, weapon type %d)",
			         hand.ready.reached ? "done - the game shows the wanted state"
			                              : "given up - the game did not follow within 2.5 s",
			         static_cast<double>(state.readySeconds), static_cast<int>(game::ReadPlayerAction()),
			         static_cast<int>(weaponType));
		}
	
		// The staff's draw and sheathe sounds, which the game
		// does not play (vr::DrawSoundForm): once the weapon shows drawn or
		// sheathed, the sound of the change.
		{
			SInt32 soundType = -1;
			game::EquippedWeaponForm(&soundType);
			const game::WeaponState weaponNow = game::ReadPlayerWeaponState();
			if (weaponNow != game::WeaponState::Unknown && state.lastWeapon != game::WeaponState::Unknown &&
			    weaponNow != state.lastWeapon && !menuIsUp) {
				// Sheathed, the weapon is still equipped: its own type.
				const SInt32 type = soundType >= 0 ? soundType : state.lastType;
				const UInt32 form = vr::DrawSoundForm(type, weaponNow == game::WeaponState::Drawn);
				if (form != 0) {
					const bool played = game::PlaySoundForm(form);
					if (state.soundLines > 0) {
						--state.soundLines;
						OBVR_LOG("Hands: the %s's %s sound (%08X) %s", vr::IsStaffWeaponType(type) ? "staff" : "weapon",
						         weaponNow == game::WeaponState::Drawn ? "draw" : "sheathe", form,
						         played ? "played" : "not played");
					}
				}
			}
			state.lastWeapon = weaponNow;
			state.lastType = soundType;
		}
	
		// Every click the laser sends into a game menu, the first several dozen: a message box
		// that could not be clicked away (2026-09-27) left no trace of whether a click was sent.
		LogDropPress(state, hand.dropGate);
		if (controls.menuClick && !state.clickWasSent && menuIsUp && state.clickLinesLeft > 0) {
			--state.clickLinesLeft;
			char tileName[64] = {};
			game::ActiveTileName(tileName, sizeof(tileName));
			OBVR_LOG("Hands: click sent to the %s menu (on top: %s) at cursor %.0f,%.0f, the tile under it \"%s\"",
			         game::MenuIdName(game::ActiveMenuId()), game::MenuIdName(game::TopVisibleMenu()),
			         static_cast<double>(frame.cursorX), static_cast<double>(frame.cursorY), tileName);
		}
		// The click's down-edge and its tile: what the SteamVR keyboard opens on (vr/VrKeyboard.h).
		state.menuClickEdge = controls.menuClick && !state.clickWasSent && menuIsUp;
		if (!state.menuClickEdge || !game::ActiveTileName(state.menuClickTile, sizeof(state.menuClickTile))) {
			state.menuClickTile[0] = '\0';
		}
		if (state.menuClickEdge) {
			game::NoteMenuClickTile(state.menuClickTile);  // the Options menu's VR Options and VR Guide
		}
		// A tick in the pointing hand as the laser comes onto one of the
		// game's tiles, a firmer one as it clicks (vr/MenuHaptics.h).
		{
			const bool pointing = menuIsUp && (hand.laserHit || hand.pokeHover);
			const vr::MenuPulse pulse =
				vr::StepTileHaptic(state.menuHaptic, pointing, pointing ? game::ActiveTile() : 0, state.menuClickEdge);
			shared.menuPulse(hand.laserRight, pulse);
		}
		state.clickWasSent = controls.menuClick;
		LogTabletWalk(state, hand, frame, state.menuClickEdge);  // and the page by the right stick
		LogJumpAndBoundFires(state, controls.jump, hand);
		// The menu key after a death: whether the game had shown a menu of
		// its own by then (the tester, 2026-10-08: "das Esc menü kam nicht,
		// musste es mit B herholen" - his logs cannot tell a menu he opened
		// from one the game opened).
		{
			const bool dead = game::PlayerIsDead();
			state.deadSeconds = dead ? state.deadSeconds + links.deltaSeconds : 0.0f;
			if (controls.menu && !state.menuWasSent && dead && state.deathMenuLines > 0) {
				--state.deathMenuLines;
				OBVR_LOG("Hands: the menu key %.1f s after death - the game %s", static_cast<double>(state.deadSeconds),
				         menuIsUp ? "had a menu up already" : "had shown no menu of its own by then");
			}
			state.menuWasSent = controls.menu;
		}
		// The left stick's analog walk (vr/AnalogWalkLogic.h, game/AnalogWalk.h):
		// the engine's pace factor written for the push, the heading kept for
		// the player's move, and the run key turned over under Always Run so
		// the legs show the gait the push chose.
		StepAnalogWalk(links.locomotion, handStep, handShared, flying, links.deltaSeconds, links.rolesSwapped, controls);
		game::ApplyHandControls(controls, config.handKeys, config.hands.turnSpeed);
		state.controlsHeld = true;
		// Not while the sticks walk the player under a reached container's
		// menu: the player's input would take the steps as a turn of the
		// body. Under the tablet it reads none (game/MenuPause.h): they go on.
		if ((hand.laserHit || hand.pokeHover || hand.cursorMade) && (!hand.walkingUnderMenu || frame.walkUnderTablet) &&
		    (hand.cursorDx != 0 || hand.cursorDy != 0)) {
			game::MoveMouseBy(hand.cursorDx, hand.cursorDy);
			// Whether the steps arrive: the engine's cursor read the same
			// through many frames of steps means the mouse moves are not
			// reaching it (the tester, 2026-10-07: "kann im hauptmenü nicht
			// mehr buttons anvisieren oder klicken" with the cursor at 0,0
			// through every step in the log).
			if (frame.cursorValid && frame.cursorX == state.stuckX && frame.cursorY == state.stuckY) {
				++state.stuckFrames;
			} else {
				state.stuckFrames = 0;
				state.stuckX = frame.cursorX;
				state.stuckY = frame.cursorY;
			}
			if (state.stuckFrames == 90 && !state.stuckSaid) {
				state.stuckSaid = true;
				OBVR_LOG("Menu cursor: the engine's cursor stayed at %.0f,%.0f through 90 frames of mouse steps - the "
				         "moves are not reaching the game (its window not in front, or its cursor clamped)",
				         static_cast<double>(frame.cursorX), static_cast<double>(frame.cursorY));
			}
		}
		if (hand.menuScroll != 0) {
			game::ScrollMouseWheel(hand.menuScroll);
			// Said a few times: the headset run of 2026-09-25 had the lists not
			// scroll, and whether the notches are sent at all is the first
			// question.
			if (state.wheelLinesLeft > 0) {
				--state.wheelLinesLeft;
				OBVR_LOG("Hands: mouse wheel %+d notch(es) sent to the %s menu at cursor %.0f,%.0f",
				         hand.menuScroll, game::MenuIdName(game::ActiveMenuId()),
				         static_cast<double>(frame.cursorX), static_cast<double>(frame.cursorY));
			}
		}
		if (hand.pokePress && state.pokeLinesLeft > 0) {
			--state.pokeLinesLeft;
			OBVR_LOG("Hands: the finger pressed the menu at cursor %.0f, %.0f", frame.cursorX,
			         frame.cursorY);
		}
	} else if (state.controlsHeld) {
		game::ReleaseHandControls(config.handKeys);
		state.controlsHeld = false;
	}
	if (!hand.controlsActive) {
		// No hands, no analog walk: the pace factor back to the engine's.
		float pace = 1.0f;
		game::StepAnalogWalk(game::AnalogWalkRequest{}, pace);
	}
	
	PlaceMenusOnHands(state, config, backend, hand, frame, links);
}

}  // namespace obvr::camera::hands
