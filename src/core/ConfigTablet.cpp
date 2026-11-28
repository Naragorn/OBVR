#include "core/ConfigTablet.h"

#include "core/Config.h"
#include "core/ConfigRead.h"
#include "core/Log.h"

namespace obvr {

void ReadTabletStep(Config& config, const char* path) {
	using config_read::ReadBool;
	using config_read::ReadFloat;
	vr::TabletSettings& t = config.hands.tablet;
	t.enabled = ReadBool("Tablet", "Enabled", t.enabled, path);
	t.widthMetres = ReadFloat("Tablet", "WidthMetres", t.widthMetres, path);
	t.bookWidthMetres = ReadFloat("Tablet", "BookWidthMetres", t.bookWidthMetres, path);
	t.inScene = ReadBool("Tablet", "InScene", t.inScene, path);
	t.drawnFinger = ReadBool("Tablet", "DrawnFinger", t.drawnFinger, path);
	t.mirror = ReadBool("Tablet", "Mirror", t.mirror, path);
	t.mirrorDust = ReadBool("Tablet", "MirrorDust", t.mirrorDust, path);
	t.gripUp = ReadFloat("Tablet", "GripUp", t.gripUp, path);
	t.gripForward = ReadFloat("Tablet", "GripForward", t.gripForward, path);
	t.tiltDegrees = ReadFloat("Tablet", "TiltDegrees", t.tiltDegrees, path);
	t.gripShare = ReadFloat("Tablet", "GripShare", t.gripShare, path);
	char word[32] = "";
	config_read::ReadText("Tablet", "Figure", word, sizeof(word), path);
	if (word[0] != 0 && !vr::ParseTabletFigure(word, t.figure)) {
		OBVR_LOG("Config: [Tablet] Figure=%s is not beside, off or tab - kept %s", word,
		         vr::kTabletFigureNames[static_cast<UInt32>(t.figure)]);
	}
	char unroll[32] = "";
	config_read::ReadText("Tablet", "Unroll", unroll, sizeof(unroll), path);
	if (unroll[0] != 0 && !vr::ParseTabletUnroll(unroll, t.unroll)) {
		OBVR_LOG("Config: [Tablet] Unroll=%s is not sideways or vertical - kept %s", unroll,
		         vr::kTabletUnrollNames[static_cast<UInt32>(t.unroll)]);
	}
	// A page narrower than a hand or wider than both arms is no tablet.
	t.widthMetres = t.widthMetres < 0.15f ? 0.15f : (t.widthMetres > 0.8f ? 0.8f : t.widthMetres);
	t.bookWidthMetres = t.bookWidthMetres < 0.15f ? 0.15f : (t.bookWidthMetres > 0.8f ? 0.8f : t.bookWidthMetres);
	t.gripShare = t.gripShare < 0.0f ? 0.0f : (t.gripShare > 1.0f ? 1.0f : t.gripShare);
	// How it sits in the hand (vr::HeldPlace): half a turn either way at most,
	// and within an arm's reach of the palm.
	const auto within = [](float v, float most) { return v < -most ? -most : (v > most ? most : v); };
	vr::HeldPlace& h = t.held;
	h.turnDegrees = within(ReadFloat("Tablet", "HeldTurnDegrees", h.turnDegrees, path), 180.0f);
	h.leanDegrees = within(ReadFloat("Tablet", "HeldLeanDegrees", h.leanDegrees, path), 180.0f);
	h.rollDegrees = within(ReadFloat("Tablet", "HeldRollDegrees", h.rollDegrees, path), 180.0f);
	h.acrossCm = within(ReadFloat("Tablet", "HeldAcrossCm", h.acrossCm, path), 40.0f);
	h.upCm = within(ReadFloat("Tablet", "HeldUpCm", h.upCm, path), 40.0f);
	h.outCm = within(ReadFloat("Tablet", "HeldOutCm", h.outCm, path), 40.0f);
}

}  // namespace obvr
