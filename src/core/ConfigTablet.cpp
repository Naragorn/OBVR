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
	t.inScene = ReadBool("Tablet", "InScene", t.inScene, path);
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
	t.gripShare = t.gripShare < 0.0f ? 0.0f : (t.gripShare > 1.0f ? 1.0f : t.gripShare);
}

}  // namespace obvr
