#include "render/HandHudMarks.h"

#include <cstdio>

#include "core/Log.h"
#include "render/D3D9Types.h"
#include "render/LayoutProbe.h"
#include "vr/HudFringe.h"

namespace obvr::render {
namespace {

// How far out of each cut the fringes are looked for, in the panel's pixels:
// about 10 HUD units at 2.3 pixels a unit, past the text margin
// (vr::kHudTextMarginUnits) and short of most neighbours.
constexpr UInt32 kFringeBandPixels = 24;

struct FringeVisit {
	vr::HudFringe fringe[vr::kHudElementCount];
	bool wanted[vr::kHudElementCount] = {};
};

void AccumulateFringes(const UInt32* row, UInt32 width, UInt32 y, void* context) {
	auto* visit = static_cast<FringeVisit*>(context);
	for (UInt32 e = 0; e < vr::kHudElementCount; ++e) {
		if (visit->wanted[e]) {
			vr::AccumulateHudFringeRow(visit->fringe[e], row, width, y);
		}
	}
}

// One read of the panel for every wanted element: how far drawn pixels reach
// past each side of its rectangle (vr/HudFringe.h), written as
// "<element> <what> left n px, top n px, right n px, bottom n px".
void LogFringes(const char* mark, const char* what, const bool* wanted, const vr::CaptureRect* rects,
                const HandHudPanel& panel) {
	FringeVisit visit;
	bool any = false;
	for (UInt32 e = 0; e < vr::kHudElementCount; ++e) {
		visit.wanted[e] = wanted[e] && rects[e].valid;
		visit.fringe[e] = vr::StartHudFringe(rects[e], kFringeBandPixels);
		any = any || visit.wanted[e];
	}
	if (!any) {
		return;
	}
	if (!VisitSurfaceRows(panel.device, panel.surface, panel.width, panel.height, d3d9::kFormatA8R8G8B8,
	                      AccumulateFringes, &visit)) {
		OBVR_LOG("Hand HUD: at \"%s\" - the panel could not be read back (%s)", mark, what);
		return;
	}
	for (UInt32 e = 0; e < vr::kHudElementCount; ++e) {
		if (!visit.wanted[e]) {
			continue;
		}
		const vr::HudFringe& f = visit.fringe[e];
		OBVR_LOG("Hand HUD: at \"%s\" - %s %s left %u px, top %u px, right %u px, bottom %u px "
		         "(drawn pixels within %u px: %u %u %u %u; %u inside)",
		         mark, vr::kHudElementKeys[e], what, vr::HudFringeDepth(f, vr::kFringeLeft),
		         vr::HudFringeDepth(f, vr::kFringeTop), vr::HudFringeDepth(f, vr::kFringeRight),
		         vr::HudFringeDepth(f, vr::kFringeBottom), f.band, f.pixels[vr::kFringeLeft],
		         f.pixels[vr::kFringeTop], f.pixels[vr::kFringeRight], f.pixels[vr::kFringeBottom], f.inside);
	}
}

}  // namespace

void LogHandHudOverhang(const char* mark, const vr::CaptureRect* boxes, const bool* lift, const HandHudPanel& panel) {
	LogFringes(mark, "drawn past its box", lift, boxes, panel);
}

void ReportHandHudAtMark(const char* mark, const vr::HandHudSettings& settings, const HandHudLayer& layer,
                         const vr::CaptureRect* rects, const HandHudPanel& panel, bool probe) {
	for (UInt32 e = 0; e < vr::kHudElementCount; ++e) {
		OBVR_LOG("Hand HUD: at \"%s\" - %s %s, %s, pixels %d,%d..%d,%d", mark, vr::kHudElementKeys[e],
		         vr::kHudPlaceNames[static_cast<UInt32>(settings.element[e].place)],
		         layer.Lifted(e) ? "lifted" : (rects[e].valid ? "found, not lifted" : "not found"), rects[e].left,
		         rects[e].top, rects[e].right, rects[e].bottom);
	}
	if (!probe) {
		return;
	}
	bool lifted[vr::kHudElementCount];
	for (UInt32 e = 0; e < vr::kHudElementCount; ++e) {
		lifted[e] = layer.Lifted(e);
	}
	// 0 on every side is a clean cut; hand-hud-fringe.txt expects it.
	LogFringes(mark, "fringe", lifted, rects, panel);
	char name[96];
	std::snprintf(name, sizeof(name), "OBVR-HandHud-atlas-%s.bmp", mark);
	const bool atlas = layer.DumpAtlas(panel.device, name);
	std::snprintf(name, sizeof(name), "OBVR-HandHud-panel-%s.bmp", mark);
	const bool dumped = DumpSurfaceBmp(panel.device, panel.surface, panel.width, panel.height,
	                                   d3d9::kFormatA8R8G8B8, name);
	OBVR_LOG("Hand HUD: the atlas %s, the panel left behind %s", atlas ? "written" : "refused",
	         dumped ? "written" : "refused");
}

}  // namespace obvr::render
