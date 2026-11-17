#include "core/ConfigTablet.h"

#include "core/Config.h"
#include "core/ConfigRead.h"

namespace obvr {

void ReadTabletStep(Config& config, const char* path) {
	using config_read::ReadBool;
	using config_read::ReadFloat;
	vr::TabletSettings& t = config.hands.tablet;
	t.enabled = ReadBool("Tablet", "Enabled", t.enabled, path);
	t.widthMetres = ReadFloat("Tablet", "WidthMetres", t.widthMetres, path);
	t.inScene = ReadBool("Tablet", "InScene", t.inScene, path);
	t.closesInCombat = ReadBool("Tablet", "ClosesInCombat", t.closesInCombat, path);
	t.gripUp = ReadFloat("Tablet", "GripUp", t.gripUp, path);
	t.gripForward = ReadFloat("Tablet", "GripForward", t.gripForward, path);
	t.tiltDegrees = ReadFloat("Tablet", "TiltDegrees", t.tiltDegrees, path);
	t.gripShare = ReadFloat("Tablet", "GripShare", t.gripShare, path);
	// A page narrower than a hand or wider than both arms is no tablet.
	t.widthMetres = t.widthMetres < 0.15f ? 0.15f : (t.widthMetres > 0.8f ? 0.8f : t.widthMetres);
	t.gripShare = t.gripShare < 0.0f ? 0.0f : (t.gripShare > 1.0f ? 1.0f : t.gripShare);
}

}  // namespace obvr
