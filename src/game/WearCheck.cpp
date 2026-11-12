#include "game/WearCheck.h"

#include <cstdio>
#include <string>
#include <vector>

#include "core/AddressSpace.h"
#include "core/Config.h"
#include "core/Log.h"
#include "game/BladeMaterial.h"
#include "game/FirstPersonArms.h"
#include "game/FirstPersonHide.h"
#include "game/GameAddresses.h"
#include "game/GameTypes.h"
#include "game/PlayerBody.h"
#include "game/SkinRead.h"
#include "game/WearCheckLogic.h"
#include "render/DrawnShapes.h"
#include "render/InterfaceRenderHook.h"
#include "test/HandScriptRuntime.h"

namespace obvr::game {
namespace {

enum class Tree : UInt8 { Body, FirstPerson, Paperdoll };

const char* TreeName(Tree t) {
	switch (t) {
	case Tree::Body: return "body";
	case Tree::FirstPerson: return "first person";
	case Tree::Paperdoll: return "paperdoll";
	}
	return "?";
}

struct Entry {
	const NiAVObject* geometry = nullptr;
	Tree tree = Tree::Body;
	UInt32 wornId = 0;
	std::string name;
	bool shown = false;
	bool drawn = false;
	bool skinRead = false;
	HandShapeFingers fingers;
};

struct Pending {
	bool armed = false;
	std::string mark;
	std::vector<Entry> entries;
	bool refusedReported = false;
	bool paperdollSeen = false;
	std::string paperdollView;  // the crown and soles against the menu's view, read at the draw
};

Pending& P() {
	static Pending p;
	return p;
}

// A finger counts where one of its bones owns vertices of the shape and
// stands at a scale: the body's arm collapsed for the draw (HideArms) leaves
// its fingers at scale 0 however the shape is drawn.
constexpr float kStandingScale = 0.05f;
constexpr float kOwningWeight = 0.01f;

void ReadFingers(const NiAVObject* geometry, Entry& e) {
	SkinnedShape skin;
	char why[200];
	const SkinReadResult r = ReadSkinnedShape(geometry, skin, why, sizeof(why));
	e.skinRead = r != SkinReadResult::Refused;
	if (r != SkinReadResult::Skinned) {
		return;
	}
	std::vector<UInt8> owns(skin.boneName.size(), 0);
	for (const SkinWeight& w : skin.weights) {
		if (w.weight > kOwningWeight && w.bone < owns.size()) {
			owns[w.bone] = 1;
		}
	}
	for (UInt32 b = 0; b < skin.boneName.size(); ++b) {
		UInt8 side = 0;
		UInt8 finger = 0;
		if (owns[b] != 0 && FingerOfBone(skin.boneName[b], side, finger) &&
		    skin.boneNode[b]->worldTransform.scale > kStandingScale) {
			e.fingers.fingers[side] = static_cast<UInt8>(e.fingers.fingers[side] | (1u << finger));
		}
	}
}

void Snap(const NiAVObject* root, Tree tree, std::vector<Entry>& out) {
	if (!mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(root))) {
		return;
	}
	std::vector<ShapeUnder> shapes;
	CollectShapesUnder(root, shapes, 256);
	for (const ShapeUnder& s : shapes) {
		Entry e;
		e.geometry = s.geometry;
		e.tree = tree;
		e.wornId = s.wornId;
		e.name = ShapeName(s.geometry);
		e.shown = !s.hidden;
		if (tree != Tree::Paperdoll) {
			ReadFingers(s.geometry, e);
		}
		out.push_back(e);
	}
}

const NiAVObject* FindNamed(const NiAVObject* node, const char* name, UInt32 depth) {
	if (!mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(node)) || depth > 24) {
		return nullptr;
	}
	if (SameShapeName(ShapeName(node), name)) {
		return node;
	}
	if (!NiClassIsNode(NiClassNameOf(node))) {
		return nullptr;
	}
	UInt32 count = 0;
	NiAVObject* const* children = NodeChildren(node, count);
	for (UInt32 i = 0; children != nullptr && i < count; ++i) {
		if (const NiAVObject* found = FindNamed(children[i], name, depth + 1)) {
			return found;
		}
	}
	return nullptr;
}

void LogHands(const char* mark, const std::vector<Entry>& entries) {
	std::vector<HandShapeFingers> drawn;
	for (const Entry& e : entries) {
		if (e.tree != Tree::Paperdoll && e.drawn && (e.fingers.fingers[0] != 0 || e.fingers.fingers[1] != 0)) {
			drawn.push_back(e.fingers);
		}
	}
	const HandsTally t = TallyHands(drawn);
	const HandVerdict right = JudgeHand(t.side[kWearRight]);
	const HandVerdict left = JudgeHand(t.side[kWearLeft]);
	const char* verdict = HandWrong(right) || HandWrong(left) ? "WEAR WRONG"
	                      : right == HandVerdict::Ok && left == HandVerdict::Ok ? "hands ok"
	                                                                            : "hands not both drawn";
	OBVR_LOG("Wear check at %s: hands - right %u shape(s) with %u finger(s) (%s), left %u shape(s) with %u "
	         "finger(s) (%s) - %s",
	         mark, t.side[kWearRight].shapes,
	         t.side[kWearRight].shapes > 0 ? FingerCount(t.side[kWearRight].fewest) : 0u, HandVerdictWords(right),
	         t.side[kWearLeft].shapes, t.side[kWearLeft].shapes > 0 ? FingerCount(t.side[kWearLeft].fewest) : 0u,
	         HandVerdictWords(left), verdict);
}

void LogWorn(const char* mark, const std::vector<Entry>& entries, bool paperdoll) {
	std::vector<WornShapeSeen> seen;
	for (const Entry& e : entries) {
		if ((e.tree == Tree::Paperdoll) == paperdoll) {
			seen.push_back(WornShapeSeen{e.wornId, e.name.c_str(), e.shown, e.drawn});
		}
	}
	for (const WornItemTally& item : TallyWorn(seen)) {
		// the shapes no tree drew, each name once
		std::vector<const char*> notDrawn;
		for (const WornShapeSeen& s : seen) {
			bool drawnSomewhere = false;
			bool listed = false;
			for (const WornShapeSeen& o : seen) {
				drawnSomewhere = drawnSomewhere || (o.wornId == s.wornId && o.drawn && SameShapeName(o.name, s.name));
			}
			for (const char* n : notDrawn) {
				listed = listed || SameShapeName(n, s.name);
			}
			if (s.wornId == item.wornId && !drawnSomewhere && !listed) {
				notDrawn.push_back(s.name);
			}
		}
		std::string missing;
		for (const char* n : notDrawn) {
			missing += missing.empty() ? "" : ", ";
			missing += n;
		}
		// the verdict first, so a scenario's expect line reads the same for
		// any count of shapes
		if (EveryShapeDrawn(item)) {
			OBVR_LOG("Wear check at %s: item %08X on the %s - every shape drawn (%u shapes, %u shown)", mark,
			         item.wornId, paperdoll ? "paperdoll" : "body", item.shapes, item.shown);
		} else {
			OBVR_LOG("Wear check at %s: item %08X on the %s - %u of %u shapes drawn, %u shown (not drawn: %s)", mark,
			         item.wornId, paperdoll ? "paperdoll" : "body", item.drawn, item.shapes, item.shown,
			         missing.c_str());
		}
	}
}

// The figure's crown and soles against the menu's view, read while it is
// drawn (the camera at the origin looking along +y, z up: MenuModelFit.h).
std::string DescribePaperdollView(const NiAVObject* model) {
	const NiAVObject* const head = FindNamed(model, "Bip01 Head", 0);
	const NiAVObject* const footL = FindNamed(model, "Bip01 L Foot", 0);
	const NiAVObject* const footR = FindNamed(model, "Bip01 R Foot", 0);
	const float yScale = render::MenuModelYScale();
	char line[400];
	if (head == nullptr || footL == nullptr || footR == nullptr || !(yScale > 0.01f)) {
		std::snprintf(line, sizeof(line),
		              "the paperdoll's view not read (head %d, feet %d %d, the view's y scale %.3f) - WEAR WRONG",
		              head != nullptr ? 1 : 0, footL != nullptr ? 1 : 0, footR != nullptr ? 1 : 0,
		              static_cast<double>(yScale));
		return line;
	}
	// The crown over the head bone and the soles under the feet's bones, in
	// a human of scale 1 (derived from Oblivion's 128-unit body: the head
	// bone near 114, the feet's near 9) - the model's own scale applied.
	const float scale = model->worldTransform.scale > 0.0f ? model->worldTransform.scale : 1.0f;
	const NiPoint3 top = head->worldTransform.pos + NiPoint3{0.0f, 0.0f, 14.0f * scale};
	const NiPoint3& lowFoot =
	    footL->worldTransform.pos.z < footR->worldTransform.pos.z ? footL->worldTransform.pos : footR->worldTransform.pos;
	const NiPoint3 bottom = lowFoot - NiPoint3{0.0f, 0.0f, 10.0f * scale};
	const float tanHalf = 1.0f / yScale;
	const FigureInView v = JudgeFigureInView(top.z, top.y, bottom.z, bottom.y, tanHalf);
	std::snprintf(line, sizeof(line),
	              "the paperdoll's crown at %.1f %.1f %.1f, soles at %.1f %.1f %.1f, the view %.3f up and down (as "
	              "tangents: crown %.3f, soles %.3f) - %s",
	              static_cast<double>(top.x), static_cast<double>(top.y), static_cast<double>(top.z),
	              static_cast<double>(bottom.x), static_cast<double>(bottom.y), static_cast<double>(bottom.z),
	              static_cast<double>(tanHalf), static_cast<double>(top.y > 0.0f ? top.z / top.y : 0.0f),
	              static_cast<double>(bottom.y > 0.0f ? bottom.z / bottom.y : 0.0f),
	              v.top && v.bottom ? "the paperdoll head to feet in view"
	                                : !v.top ? "WEAR WRONG: the paperdoll's head out of view"
	                                         : "WEAR WRONG: the paperdoll's feet out of view");
	return line;
}

// The recorder's call at the figure's first draw of the record.
void PaperdollSeen(const void* model) {
	Pending& p = P();
	if (!p.armed || p.paperdollSeen) {
		return;
	}
	p.paperdollSeen = true;
	const auto* const figure = static_cast<const NiAVObject*>(model);
	Snap(figure, Tree::Paperdoll, p.entries);
	p.paperdollView = DescribePaperdollView(figure);
}

void LogShapes(const char* mark, const std::vector<Entry>& entries) {
	UInt32 lines = 0;
	for (const Entry& e : entries) {
		if ((e.wornId == 0 && e.fingers.fingers[0] == 0 && e.fingers.fingers[1] == 0 && e.tree != Tree::Paperdoll) ||
		    lines >= 64) {
			continue;
		}
		++lines;
		OBVR_LOG("Wear check shape at %s: %s \"%s\" item %08X %s %s, fingers right %02X left %02X%s", mark,
		         TreeName(e.tree), e.name.c_str(), e.wornId, e.shown ? "shown" : "hidden",
		         e.drawn ? "drawn" : "not drawn", e.fingers.fingers[kWearRight], e.fingers.fingers[kWearLeft],
		         e.skinRead ? "" : " (skin not read)");
	}
}

void Judge() {
	Pending& p = P();
	render::EndDrawnShapes();
	p.armed = false;
	const char* const mark = p.mark.c_str();
	UInt32 paperdollDrawn = 0;
	for (Entry& e : p.entries) {
		// the paperdoll's shapes are its own copies: drawn by the menus,
		// outside the eyes' culls (render/DrawnShapes.h)
		e.drawn = e.tree == Tree::Paperdoll
		              ? render::DrawnElsewhere(e.geometry) || render::DrawnInOtherCull(e.geometry)
		              : render::DrawnInWorld(e.geometry);
		paperdollDrawn += e.tree == Tree::Paperdoll && e.drawn ? 1u : 0u;
	}
	OBVR_LOG("Wear check at %s: %u shapes drawn by the eyes' passes, %u by the other passes, %u in held culls "
	         "(shadow maps, reflection, the menus); %u shapes read, the paperdoll %s (%u of its shapes drawn, %u draws)",
	         mark, render::DrawnWorldCount(), render::DrawnElsewhereCount(), render::DrawnHeldCount(),
	         static_cast<UInt32>(p.entries.size()), p.paperdollSeen ? "drawn" : "not drawn", paperdollDrawn,
	         render::DrawnPaperdollDraws());
	LogShapes(mark, p.entries);
	LogHands(mark, p.entries);
	LogWorn(mark, p.entries, false);
	if (paperdollDrawn > 0) {
		LogWorn(mark, p.entries, true);
		OBVR_LOG("Wear check at %s: %s", mark, p.paperdollView.c_str());
	}
	p.entries.clear();
}

}  // namespace

void WearCheckBeforeWorld() {
	if (!GetConfig().body.wearCheck) {
		return;
	}
	Pending& p = P();
	const bool marked = test::HandScriptMarkedThisFrame();
	if (p.armed && marked && p.mark == test::HandScriptMarkName()) {
		// another world render of the mark's own frame
		render::SetDrawnShapesWorld(true);
		return;
	}
	if (p.armed) {
		Judge();
	}
	if (!marked) {
		return;
	}
	if (!render::InstallDrawnShapes()) {
		if (!p.refusedReported) {
			p.refusedReported = true;
			OBVR_LOG("Wear check: the draws cannot be recorded - WEAR WRONG");
		}
		return;
	}
	p.armed = true;
	p.mark = test::HandScriptMarkName();
	p.entries.clear();
	p.paperdollSeen = false;
	p.paperdollView.clear();
	render::SetDrawnPaperdollSeen(&PaperdollSeen);
	Snap(PlayerBodyRoot(), Tree::Body, p.entries);
	Snap(FirstPersonArmsNode(), Tree::FirstPerson, p.entries);
	render::BeginDrawnShapes();
	render::SetDrawnShapesWorld(true);
}

void WearCheckAfterWorld() { render::SetDrawnShapesWorld(false); }

}  // namespace obvr::game
