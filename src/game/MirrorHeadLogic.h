#pragma once

// The player's head in the mirror and nowhere else (docs/mirror-spec.md 4 and
// 13, stage 3). The eyes' renders see the head collapsed: Bip01 Head's world
// transform at scale 0 ([Body] HideHead, game/PlayerBody.cpp
// CollapseForThisDraw) and, with [Body] CullHiddenHead, the two FaceGen nodes'
// hidden bit (game/HeadCull.cpp). The mirror's render runs after both eyes'
// captures (render/MirrorGlassPass.cpp), so the head is shown for it alone:
//
// 1. Bip01 Head's world transforms recomputed at its own scale (the collapse
//    put the local scale back), so the FaceGen parts and anything hanging
//    under the head stand where they belong.
// 2. The hidden bits this code set on the FaceGen nodes taken off (a bit
//    someone else set stays).
// 3. The bone palettes of every skin drawn with the head's bones built again
//    for the mirror's render. The palette builder 0x007655F0 rebuilds a skin
//    only when its stamp (NiSkinInstance +0x18) differs from the renderer's
//    (read: 0x00765608 against 0x00422DE0 on [renderer+0x8B0]); a skin built
//    earlier in the frame with the collapsed head would keep its zero rows.
//    The stamp is turned to a value no renderer stamp has, so the builder
//    makes the rows again from the head's true transform.
//
// After the render, in reverse: the stamps turned again (the rows now carry
// the shown head; whatever draws the skin next builds them from the collapse),
// the head collapsed again, the bits put back on.
//
// Pure, over plain values; game/MirrorHead.cpp reads and writes the nodes.
// mirror_head_test covers every flow.

#include <cstdint>

namespace obvr::game {

// Why the head is not switched for a mirror's render, in the order asked.
enum class MirrorHeadRefusal : std::uint8_t {
	None,          // switched: the head shows in the mirror
	NoBody,        // the third-person root is not read
	BodyHidden,    // the body is not drawn this frame ([Body] Visible=0, a menu): hands and room only
	NoHead,        // Bip01 Head is not under the root
	AlreadyShown,  // the head is not collapsed this frame ([Body] HideHead=0): the mirror shows it as it is
	BadScale,      // the head's own scale is not a positive number: no true transform to give it
	TooManySkins,  // more skins on the head's bones than the list holds: a partial head would be worse
	Count
};

inline const char* MirrorHeadRefusalName(MirrorHeadRefusal r) {
	switch (r) {
	case MirrorHeadRefusal::None: return "shown";
	case MirrorHeadRefusal::NoBody: return "no body";
	case MirrorHeadRefusal::BodyHidden: return "body not drawn";
	case MirrorHeadRefusal::NoHead: return "no head bone";
	case MirrorHeadRefusal::AlreadyShown: return "not hidden";
	case MirrorHeadRefusal::BadScale: return "bad scale";
	case MirrorHeadRefusal::TooManySkins: return "too many skins";
	case MirrorHeadRefusal::Count: break;
	}
	return "unknown";
}

// The skins drawn with the head's bones (its own or a bone under it) under the
// body's root, each with the stamp it had: a fixed list, no allocation in the
// pass. A face, its hair, eyes, ears, mouth, teeth and tongue, a helmet or
// hood, the upper body's neck ring: about ten (Debug.BodyTreeProbe's tree,
// HeadCull.cpp's parts) [D].
inline constexpr std::uint32_t kMirrorHeadSkinsMax = 32;

struct MirrorHeadSkins {
	std::uint32_t count = 0;
	bool overflow = false;
	std::uintptr_t skins[kMirrorHeadSkinsMax] = {};
};

// One skin found: kept once (several shapes can share a skin instance), and
// overflow when the list is full. A null skin is no skin.
inline void AddMirrorHeadSkin(MirrorHeadSkins& list, std::uintptr_t skin) {
	if (skin == 0) {
		return;
	}
	for (std::uint32_t i = 0; i < list.count; ++i) {
		if (list.skins[i] == skin) {
			return;
		}
	}
	if (list.count >= kMirrorHeadSkinsMax) {
		list.overflow = true;
		return;
	}
	list.skins[list.count++] = skin;
}

// A stamp no renderer stamp is: the builder sees a skin not built this render
// and builds it (the top bit turned; a counter or an identity word does not
// come back to it within a frame) [D].
inline std::uint32_t StaleMirrorStamp(std::uint32_t stamp) { return stamp ^ 0x80000000u; }

struct MirrorHeadInputs {
	bool bodyRead = false;
	bool bodyDrawn = false;  // the root's hidden bit off this frame
	bool headRead = false;
	float headWorldScale = 1.0f;  // 0 while collapsed for the draw
	float headLocalScale = 1.0f;
	bool cullOurs[2] = {false, false};  // the FaceGen nodes' bits HeadCull.cpp set this render
	std::uint32_t skins = 0;
	bool skinsOverflow = false;
};

// What the pass switches, and so what it puts back.
struct MirrorHeadPlan {
	MirrorHeadRefusal refusal = MirrorHeadRefusal::NoBody;
	bool restoreHead = false;              // the true transform now, the collapse after
	bool liftCull[2] = {false, false};     // the bits off now, on after
	std::uint32_t staleSkins = 0;          // stamps turned now and after
};

inline MirrorHeadPlan PlanMirrorHead(const MirrorHeadInputs& in) {
	MirrorHeadPlan plan;
	if (!in.bodyRead) {
		plan.refusal = MirrorHeadRefusal::NoBody;
		return plan;
	}
	if (!in.bodyDrawn) {
		plan.refusal = MirrorHeadRefusal::BodyHidden;
		return plan;
	}
	if (!in.headRead) {
		plan.refusal = MirrorHeadRefusal::NoHead;
		return plan;
	}
	if (!(in.headWorldScale == 0.0f)) {
		plan.refusal = MirrorHeadRefusal::AlreadyShown;
		return plan;
	}
	if (!(in.headLocalScale > 0.0f) || !(in.headLocalScale < 1.0e6f)) {
		plan.refusal = MirrorHeadRefusal::BadScale;
		return plan;
	}
	if (in.skinsOverflow) {
		plan.refusal = MirrorHeadRefusal::TooManySkins;
		return plan;
	}
	plan.refusal = MirrorHeadRefusal::None;
	plan.restoreHead = true;
	plan.liftCull[0] = in.cullOurs[0];
	plan.liftCull[1] = in.cullOurs[1];
	plan.staleSkins = in.skins;
	return plan;
}

}  // namespace obvr::game
