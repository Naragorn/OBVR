#include "game/SpellOrb.h"

#include <cstring>

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/BonePin.h"
#include "game/FirstPersonHide.h"
#include "game/GameCamera.h"
#include "game/GameTypes.h"
#include "game/OwnedNode.h"

namespace obvr::game {
namespace {

constexpr UInt16 kAppCulledBit = 0x0001;
constexpr UInt32 kLapseFrames = 2;
// The model's sequence the engine plays on the flying bolt (0x006950C4): the fireball's core and its
// flames in the model's own space, so they stay with the hand. The hand effect's "SpecialIdle_Cast"
// (0x005EDE12) is only a flame in world space, emitted where the animation had the hand before the
// pins - not seen at the controller (harness 2026-11-23).
constexpr const char* kSequence = "SpecialIdle_Projectile";
// The casting hand (the game's left; a left-hander's skeleton is drawn mirrored) and its middle
// finger's base, for the palm's middle (game/BonePin.h PalmCentre), as CameraHook puts magicNode.
constexpr const char* kHandBone = "Bip01 L Hand";
constexpr const char* kKnuckleBone = "Bip01 L Finger2";

struct Orb {
	NiAVObject* node = nullptr;  // OBVR's copy, with a reference of its own
	char path[260] = {};         // the model it is a copy of
	bool wanted = false;
	float scale = 0.0f;
	UInt32 frame = 0;
	UInt32 wantedAt = 0;
	bool full = false;
	bool shownWas = false;
	bool fullSaid = false;
	UInt32 lines = 16;
};

Orb& State() {
	static Orb orb;
	return orb;
}

void Say(Orb& o, const char* what, const char* path) {
	if (o.lines > 0) {
		--o.lines;
		OBVR_LOG("Spell orb: %s (%s)", what, path != nullptr && path[0] != '\0' ? path : "no model");
	}
}

void Hide(Orb& o) {
	if (o.node != nullptr) {
		o.node->flags = static_cast<UInt16>(o.node->flags | kAppCulledBit);
	}
}

// Lets the copy go: off `holder` when it still hangs there (a parent that is no longer the holder
// may be gone with an old skeleton and is not written to), OBVR's reference given back.
void Drop(Orb& o, NiAVObject* holder) {
	if (o.node == nullptr) {
		return;
	}
	Hide(o);
	if (holder != nullptr && o.node->parent == holder) {
		RemoveChildNode(holder, o.node);
	}
	ReleaseNode(o.node);
	o.node = nullptr;
	o.path[0] = '\0';
}

// The copy of `path` on `holder`: kept when it is one already, made otherwise.
bool Ensure(Orb& o, NiAVObject* holder, const char* path) {
	if (o.node != nullptr && (o.node->parent != holder || std::strcmp(o.path, path) != 0)) {
		Drop(o, holder);
	}
	if (o.node != nullptr) {
		return true;
	}
	const char* why = "";
	NiAVObject* const copy = LoadModelCopy(path, why);
	if (copy == nullptr) {
		Say(o, why, path);
		return false;
	}
	AddChildNode(holder, copy);
	if (!SettleNewNode(copy)) {
		RemoveChildNode(holder, copy);
		ReleaseNode(copy);
		Say(o, "the scene's updates are not as read - nothing in the hand", path);
		return false;
	}
	o.node = copy;
	std::strncpy(o.path, path, sizeof(o.path) - 1);
	o.path[sizeof(o.path) - 1] = '\0';
	Say(o, "the spell's model made for the casting hand, on its bone", path);
	return true;
}

}  // namespace

void WantSpellOrb(bool shown, float scale, bool full) {
	Orb& o = State();
	o.wanted = shown;
	o.scale = scale;
	o.full = full;
	o.wantedAt = o.frame;
}

void StepSpellOrb(const char* modelPath) {
	Orb& o = State();
	++o.frame;
	const bool shown = o.wanted && o.frame <= o.wantedAt + kLapseFrames;
	if (!shown) {
		if (o.shownWas) {
			Say(o, "gone from the hand", o.path);
		}
		o.shownWas = false;
		o.fullSaid = false;
		Hide(o);
		return;
	}
	// On the casting hand's bone, at its palm: the pins move the bone just before the draw
	// (BeforeFirstScenePass), after this step, and carry the model along. magicNode is no place for it:
	// the engine empties that node each frame no spell is cast (0x005F42EF).
	NiAVObject* const holder = FindFirstPersonNode(kHandBone);
	const NiAVObject* const knuckle = FindFirstPersonNode(kKnuckleBone);
	if (holder == nullptr || knuckle == nullptr || modelPath == nullptr || modelPath[0] == '\0') {
		if (!o.shownWas) {
			Say(o, holder == nullptr || knuckle == nullptr ? "no casting hand bones in the first-person skeleton - nothing in the hand"
			                                               : "the spell has no model - nothing in the hand",
			    modelPath);
		}
		o.shownWas = true;  // said once per charge
		Hide(o);
		return;
	}
	if (!Ensure(o, holder, modelPath)) {
		o.shownWas = true;
		return;
	}
	const float parentScale = holder->worldTransform.scale > 0.0f ? holder->worldTransform.scale : 1.0f;
	BonePose wanted;
	wanted.rot = holder->worldTransform.rot;
	wanted.pos = PalmCentre(holder->worldTransform.pos, knuckle->worldTransform.pos);
	const BonePose local = LocalUnderParent(holder->worldTransform.rot, holder->worldTransform.pos, parentScale, wanted);
	o.node->localTransform.rot = local.rot;
	o.node->localTransform.pos = local.pos;
	o.node->localTransform.scale = o.scale / parentScale;
	o.node->flags = static_cast<UInt16>(o.node->flags & ~kAppCulledBit);
	if (!o.shownWas) {
		// Its flames started anew each charge.
		const char* why = "";
		const bool playing = PlayNodeSequence(o.node, kSequence, why);
		if (o.lines > 0) {
			--o.lines;
			OBVR_LOG("Spell orb: \"%s\" %s", kSequence, playing ? "playing" : why);
		}
	}
	UpdateNodeTransforms(o.node);
	const bool full = o.full;
	if ((!o.shownWas || (full && !o.fullSaid)) && o.lines > 0) {
		--o.lines;
		OBVR_LOG("Spell orb: shown in the hand at %.2f of its size%s, at %.1f %.1f %.1f",
		         static_cast<double>(o.scale), full ? " (full)" : "", static_cast<double>(o.node->worldTransform.pos.x),
		         static_cast<double>(o.node->worldTransform.pos.y), static_cast<double>(o.node->worldTransform.pos.z));
	}
	o.fullSaid = full;
	o.shownWas = true;
}

}  // namespace obvr::game
