#include "game/NeckBleed.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/ActorRoster.h"
#include "game/Blood.h"
#include "game/Dismember.h"
#include "game/GameTypes.h"
#include "game/NeckBleedLogic.h"
#include "game/RagdollHold.h"
#include "game/RefFingerprint.h"

namespace obvr::game {
namespace {

// Where each wound sits: on the neck's bone, in its frame.
struct WoundPlace {
	UInt32 node = 0;
	NiPoint3 localPoint{0.0f, 0.0f, 0.0f};
	NiPoint3 localNormal{0.0f, 0.0f, 1.0f};
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
	RagdollPart part;
	const char* why = "";
	if (!FindRagdollPart(n.actor, BodyPart::Neck, n.point, part, &why)) {
		part.node = 0;
	}
	const NiTransform* t = NodeTransform(part.node);
	Wounds& w = W();
	const int at = AddNeckWound(w.logic, settings, n.actor, CurrentWorld(), !dead);
	if (at < 0) {
		return;
	}
	WoundPlace& p = w.place[at];
	p.node = t != nullptr ? part.node : 0;
	if (t != nullptr && t->scale > 0.0f) {
		const NiMatrix33 inv = InverseMatrix(t->rot);
		p.localPoint = inv * (n.point - t->pos) * (1.0f / t->scale);
		p.localNormal = inv * n.normal;
	} else {
		p.localPoint = n.point;  // no bone: kept in the world
		p.localNormal = n.normal;
	}
	if (w.lines > 0) {
		--w.lines;
		OBVR_LOG("Neck wound: %08X opened by a %s (%s, %s)%s - %u open", n.actor,
		         n.verdict.kind == BladeEntryKind::Stab ? "stab" : "slash", BodyMaterialName(n.material),
		         dead ? "dead" : "alive", p.node != 0 ? "" : ", no neck bone found - kept where it was",
		         OpenNeckWounds(w.logic));
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
			const NiTransform* t = NodeTransform(p.node);
			NiPoint3 at = p.localPoint;
			NiPoint3 out = p.localNormal;
			if (t != nullptr) {
				at = t->pos + t->rot * (p.localPoint * t->scale);
				out = t->rot * p.localNormal;
			}
			const bool sprayed = SprayBlood(actor, at + out * 2.0f, out);
			const bool stained = wound.spurts == 1 && StainBlood(actor, at + out * 2.0f, out * -1.0f);
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
