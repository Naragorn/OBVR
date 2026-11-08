#include "game/NeckBleed.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/ActorRoster.h"
#include "game/Blood.h"
#include "game/Dismember.h"
#include "game/FirstPersonHide.h"
#include "game/GameAddresses.h"
#include "game/GameTypes.h"
#include "game/NeckBleedLogic.h"
#include "game/RagdollHold.h"
#include "game/RefFingerprint.h"

namespace obvr::game {
namespace {

// Where each wound sits: on the neck's bone, in its frame; the actor's 3D
// it was read under (NeckAnchorHolds).
struct WoundPlace {
	UInt32 node = 0;
	UInt32 root = 0;
	NeckAnchor anchor = NeckAnchor::World;
	NiPoint3 localPoint{0.0f, 0.0f, 0.0f};
	NiPoint3 localNormal{0.0f, 0.0f, 1.0f};
	NiPoint3 worldPoint{0.0f, 0.0f, 0.0f};  // where it opened, for a bone no longer there
	NiPoint3 worldNormal{0.0f, 0.0f, 1.0f};
};

struct Wounds {
	NeckWounds logic;
	WoundPlace place[kNeckWoundsMax];
	UInt32 lines = 120;
};

Wounds& W() {
	static Wounds w;
	return w;
}

const char* EndName(NeckWoundEnd e) {
	switch (e) {
	case NeckWoundEnd::Spent: return "its pulses spent";
	case NeckWoundEnd::HeartStopped: return "the heart stopped";
	case NeckWoundEnd::Load: return "a load";
	case NeckWoundEnd::Gone: return "its actor unloaded";
	case NeckWoundEnd::Off: return "switched off";
	default: return "-";
	}
}

const NiTransform* NodeTransform(UInt32 node) {
	return mem::LooksLikeObjectAddress(node) ? &reinterpret_cast<const NiAVObject*>(node)->worldTransform : nullptr;
}

UInt32 ActorRoot(UInt32 actor) {
	return mem::LooksLikeObjectAddress(actor) ? *reinterpret_cast<const UInt32*>(actor + addr::kRefNiNodeOffset) : 0u;
}

const char* AnchorName(NeckAnchor a) {
	switch (a) {
	case NeckAnchor::Ragdoll: return "kept on the neck's Havok body";
	case NeckAnchor::NeckBone: return "kept on the skeleton's neck bone";
	case NeckAnchor::HeadBone: return "kept on the head bone (no neck bone)";
	case NeckAnchor::SpineBone: return "kept on the upper spine (no neck or head bone)";
	default: return "no bone found - kept where it was";
	}
}

// The wound's anchor: the neck's Havok body, else the skeleton's bones by
// name (game/NeckBleedLogic.h PickNeckAnchor).
UInt32 FindNeckAnchor(UInt32 actor, const NiPoint3& at, UInt32 root, NeckAnchor& anchor) {
	RagdollPart part;
	const char* why = "";
	const bool ragdoll = FindRagdollPart(actor, BodyPart::Neck, at, part, &why) && NodeTransform(part.node) != nullptr;
	static const char* const kNames[3] = {"Bip01 Neck", "Bip01 Head", "Bip01 Spine2"};
	NiAVObject* bones[3] = {};
	if (!ragdoll && mem::LooksLikeObjectAddress(root)) {
		FindNodesNamed(reinterpret_cast<NiAVObject*>(root), kNames, 3, bones);
	}
	const auto has = [&](UInt32 i) { return bones[i] != nullptr && NodeTransform(reinterpret_cast<UInt32>(bones[i])); };
	anchor = PickNeckAnchor(ragdoll, has(0), has(1), has(2));
	switch (anchor) {
	case NeckAnchor::Ragdoll: return part.node;
	case NeckAnchor::NeckBone: return reinterpret_cast<UInt32>(bones[0]);
	case NeckAnchor::HeadBone: return reinterpret_cast<UInt32>(bones[1]);
	case NeckAnchor::SpineBone: return reinterpret_cast<UInt32>(bones[2]);
	default: return 0;
	}
}

}  // namespace

void NoteNeckEntry(const BladeEntryNow& n, const NeckBleedSettings& settings) {
	if (!n.verdict.fresh || n.actor == 0) {
		return;
	}
	const bool headOff = HeadIsCut(n.actor);
	if (!NeckWoundOpens(settings, n.verdict.entered, n.part, n.material, n.verdict.byPressure, headOff)) {
		return;
	}
	bool dead = false;
	if (!ActorInWorld(n.actor, dead)) {
		return;
	}
	const UInt32 root = ActorRoot(n.actor);
	NeckAnchor anchor = NeckAnchor::World;
	const UInt32 node = FindNeckAnchor(n.actor, n.point, root, anchor);
	const NiTransform* const bone = NodeTransform(node);
	const NiTransform* t = bone != nullptr && bone->scale > 0.0f ? bone : nullptr;
	Wounds& w = W();
	const int at = AddNeckWound(w.logic, settings, n.actor, CurrentWorld(), !dead);
	if (at < 0) {
		return;
	}
	WoundPlace& p = w.place[at];
	p.node = t != nullptr ? node : 0;
	p.root = root;
	p.anchor = t != nullptr ? anchor : NeckAnchor::World;
	p.worldPoint = n.point;
	p.worldNormal = n.normal;
	if (t != nullptr) {
		const NiMatrix33 inv = InverseMatrix(t->rot);
		p.localPoint = inv * (n.point - t->pos) * (1.0f / t->scale);
		p.localNormal = inv * n.normal;
	} else {
		p.localPoint = n.point;  // no bone: kept in the world
		p.localNormal = n.normal;
	}
	if (w.lines > 0) {
		--w.lines;
		OBVR_LOG("Neck wound: %08X opened by a %s (%s, %s), %s - %u open", n.actor,
		         n.verdict.kind == BladeEntryKind::Stab ? "stab" : "slash", BodyMaterialName(n.material),
		         dead ? "dead" : "alive", AnchorName(p.anchor), OpenNeckWounds(w.logic));
	}
}

void StepNeckWounds(const NeckBleedSettings& settings, float dtSeconds) {
	Wounds& w = W();
	const UInt32 world = CurrentWorld();
	for (UInt32 i = 0; i < kNeckWoundsMax; ++i) {
		NeckWound& wound = w.logic.w[i];
		if (!wound.open) {
			continue;
		}
		const UInt32 actor = wound.actor;
		NeckWoundFrame f;
		f.worldNow = world;
		f.dtSeconds = dtSeconds;
		bool dead = false;
		f.there = wound.world == world && ActorInWorld(actor, dead);
		f.alive = !dead;
		const UInt32 spurtsBefore = wound.spurts;
		const NeckWoundStep r = StepNeckWound(wound, settings, f);
		if (r.spurt) {
			const WoundPlace& p = w.place[i];
			const bool holds = NeckAnchorHolds(p.root, ActorRoot(actor), p.node);
			const NiTransform* t = holds ? NodeTransform(p.node) : nullptr;
			NiPoint3 at = p.worldPoint;
			NiPoint3 out = p.worldNormal;
			if (t != nullptr) {
				at = t->pos + t->rot * (p.localPoint * t->scale);
				out = t->rot * p.localNormal;
			}
			const bool sprayed = SprayBlood(actor, at + out * 2.0f, out);
			const bool stained = wound.spurts == 1 && StainBodyBlood(actor, at + out * 2.0f, out * -1.0f);
			if (w.lines > 0 && (wound.spurts <= 3 || wound.spurts % 5 == 0)) {
				--w.lines;
				OBVR_LOG("Neck wound: %08X spurt %u (%s)%s%s", actor, wound.spurts, f.alive ? "alive" : "dead",
				         sprayed ? "" : " - no spray laid", wound.spurts == 1 ? (stained ? ", a stain" : ", no stain") : "");
			}
		}
		if (r.end != NeckWoundEnd::None && w.lines > 0) {
			--w.lines;
			OBVR_LOG("Neck wound: %08X stops - %s, after %u spurt(s)", actor, EndName(r.end), spurtsBefore);
		}
	}
}

}  // namespace obvr::game
