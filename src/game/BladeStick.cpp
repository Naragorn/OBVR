#include "game/BladeStick.h"

#include <cstdio>

#include "core/AddressSpace.h"
#include "core/Config.h"
#include "core/Log.h"
#include "core/MathFns.h"
#include "game/ActorRoster.h"
#include "game/BladeStickLogic.h"
#include "game/GameAddresses.h"
#include "game/GameTypes.h"
#include "game/Parry.h"
#include "game/RagdollHold.h"
#include "game/RefFingerprint.h"
#include "game/Shove.h"
#include "game/SlowApproach.h"

namespace obvr::game {
namespace {

// The engine's knock-state get-up waits from a time stamp
// (game::StampProcessTime); stamped again this often while they hang.
constexpr float kStampEverySeconds = 1.0f;
// The keyframed motion type (GrabPhysics.h): the ragdoll not on yet.
constexpr UInt32 kMotionKeyframed = 6;

struct Stuck {
	BladeStickState state;
	RagdollPart part;
	RagdollDrive drive;
	bool held = false;     // HoldStill and SetHeldFoe are ours
	bool knocked = false;  // the engine's knockdown asked for
	bool stepped = false;  // the step ran since the last watch
	float stampSeconds = 0.0f;
	float seconds = 0.0f;
	float deepest = 0.0f;
	UInt32 lines = 120;
	// Where they stood at the start, and the log's clock: how far a held
	// fighter moved, said each second; after the end, how far the one let go
	// moved in the next seconds.
	NiPoint3 startAt{0.0f, 0.0f, 0.0f};
	float startHeading = 0.0f;  // held while they stand on the blade: HoldStill keeps the turn
	float turned = 0.0f;        // the most the engine had turned them before it was put back, radians
	float logSeconds = 0.0f;
	UInt32 freed = 0;
	NiPoint3 freedAt{0.0f, 0.0f, 0.0f};
	float freedSeconds = 0.0f;
};

constexpr float kFreedWatchSeconds = 3.0f;

// A reference's turn about z (radians) at +0x28: its rotation is three floats
// at +0x20 (game/PlayerTeleport.cpp, kRefRotationOffset).
constexpr UInt32 kRefHeadingOffset = 0x28;

float* HeadingOf(UInt32 actor) {
	return mem::LooksLikeObjectAddress(actor) ? reinterpret_cast<float*>(actor + kRefHeadingOffset) : nullptr;
}

NiPoint3 PositionOf(UInt32 actor) {
	if (!mem::LooksLikeObjectAddress(actor)) {
		return NiPoint3{0.0f, 0.0f, 0.0f};
	}
	const float* p = reinterpret_cast<const float*>(actor + addr::kRefPositionOffset);
	return NiPoint3{p[0], p[1], p[2]};
}

float Apart(const NiPoint3& a, const NiPoint3& b) { return math::Sqrt((a - b).LengthSquared()); }

Stuck& S() {
	static Stuck s;
	return s;
}

const char* ModeName(StickMode m) { return m == StickMode::Standing ? "standing" : "hanging in its ragdoll"; }

void Say(Stuck& s, const char* text) {
	if (s.lines > 0) {
		--s.lines;
		OBVR_LOG("Blade sticks: %s", text);
	}
}

StickAnchor AnchorOf(UInt32 node) {
	StickAnchor a;
	if (!mem::LooksLikeObjectAddress(node)) {
		return a;
	}
	const NiTransform& t = reinterpret_cast<const NiAVObject*>(node)->worldTransform;
	a.valid = t.scale > 0.0f && t.scale < 100.0f;
	a.rot = t.rot;
	a.pos = t.pos;
	a.scale = t.scale;
	return a;
}

// Their legs and their blows held - unless the grapple holds someone else.
void Hold(Stuck& s, UInt32 actor) {
	const UInt32 still = HeldStillActor();
	const UInt32 foe = HeldFoe();
	if ((still != 0 && still != actor) || (foe != 0 && foe != actor)) {
		Say(s, "not held where they stand - the off hand holds someone else");
		return;
	}
	HoldStill(actor);
	SetHeldFoe(actor);
	s.held = true;
}

void LetGoOfHold(Stuck& s) {
	if (!s.held) {
		return;
	}
	if (HeldStillActor() == s.state.actor) {
		HoldStill(0);
	}
	if (HeldFoe() == s.state.actor) {
		SetHeldFoe(0);
	}
	s.held = false;
}

void End(Stuck& s, StickEnd why, UInt32 actor, const BladeStickStep* last = nullptr) {
	LetGoOfHold(s);
	char text[240];
	std::snprintf(text, sizeof(text), "%08X let go - %s, after %.1f s, at most %.1f units deep (the hand %.1f units off "
	              "the line, the blade turned to %.2f of it, %.1f units in)",
	              actor, StickEndName(why), static_cast<double>(s.seconds), static_cast<double>(s.deepest),
	              static_cast<double>(last != nullptr ? last->lateralUnits : 0.0f),
	              static_cast<double>(last != nullptr ? last->turnCos : 1.0f),
	              static_cast<double>(last != nullptr ? last->wantedDepth : 0.0f));
	Say(s, text);
	const UInt32 lines = s.lines;
	const bool watch = why != StickEnd::Load && why != StickEnd::Lost;
	s = Stuck{};
	s.lines = lines;
	s.stepped = true;
	if (watch) {
		s.freed = actor;
		s.freedAt = PositionOf(actor);
	}
}

void Start(Stuck& s, const BladeStickSettings& set, const BladeContactVerdict& v, const BladeContactFrame& f,
           const BladeEntryNow& n) {
	bool dead = false;
	const bool there = ActorInWorld(n.actor, dead);
	const StickMode mode = StickStartFor(set, n.verdict.entered && n.verdict.kind == BladeEntryKind::Stab, !dead,
	                                     n.part, n.verdict.speedMetres);
	if (mode == StickMode::None || !there) {
		return;
	}
	RagdollPart part;
	const char* why = "";
	if (!FindRagdollPart(n.actor, n.part, n.point, part, &why)) {
		char text[200];
		std::snprintf(text, sizeof(text), "%08X's %s - not stuck: %s", n.actor, BodyPartName(n.part), why);
		Say(s, text);
		return;
	}
	if (!StartBladeStick(s.state, mode, n.actor, CurrentWorld(), AnchorOf(part.node), v.pose, f.span, n.point)) {
		Say(s, "not stuck: the struck bone or the blade does not read");
		return;
	}
	s.part = part;
	s.drive = RagdollDrive{};
	s.startAt = PositionOf(n.actor);
	if (float* h = HeadingOf(n.actor)) {
		s.startHeading = *h;
	}
	s.freed = 0;
	if (mode == StickMode::Standing) {
		StaggerActor(reinterpret_cast<void*>(n.actor));
		Hold(s, n.actor);
	} else if (!dead) {
		s.knocked = KnockActorDown(reinterpret_cast<void*>(n.actor), n.point, 0.0f);
		StampProcessTime(reinterpret_cast<void*>(n.actor));
	}
	char text[220];
	std::snprintf(text, sizeof(text), "%08X's %s - stuck, %s, %.1f units in (the blade %.1f long, %.1f m/s)%s%s",
	              n.actor, BodyPartName(n.part), ModeName(mode), static_cast<double>(s.state.depth),
	              static_cast<double>(s.state.maxDepth), static_cast<double>(n.verdict.speedMetres),
	              mode == StickMode::Standing ? (s.held ? ", held where they stand" : "") : "",
	              mode == StickMode::Ragdoll && !dead ? (s.knocked ? ", taken down" : ", the knockdown refused") : "");
	Say(s, text);
}

// Killed with the blade in: the hold let go, the body hangs on the blade.
void NoteDeath(Stuck& s) {
	if (s.state.mode != StickMode::Standing) {
		return;
	}
	bool dead = false;
	if (ActorInWorld(s.state.actor, dead) && dead) {
		LetGoOfHold(s);
		BecomeRagdoll(s.state);
		Say(s, "killed with the blade in - now hanging on it");
	}
}

void Hang(Stuck& s, const BladeStickStep& r, float dt) {
	NiPoint3 centre{0.0f, 0.0f, 0.0f};
	UInt32 motion = 0;
	if (!ReadRagdollPart(s.part, centre, motion) || motion == kMotionKeyframed) {
		return;  // the ragdoll not on yet: the body waits for the engine
	}
	RagdollDriveReport report;
	const char* why = "";
	DriveRagdollPart(s.part, centre + r.shift, dt, 0.0f, s.drive, report, &why);
	bool dead = false;
	s.stampSeconds += dt;
	if (s.stampSeconds >= kStampEverySeconds && ActorInWorld(s.state.actor, dead) && !dead) {
		s.stampSeconds = 0.0f;
		StampProcessTime(reinterpret_cast<void*>(s.state.actor));
	}
}

}  // namespace

// The one let go, watched a few seconds: how far they moved (the proof the
// hold is gone). Read only in the world it ended in.
void WatchFreed(Stuck& s, float dt) {
	if (s.freed == 0) {
		return;
	}
	s.freedSeconds += dt;
	if (s.freedSeconds < kFreedWatchSeconds) {
		return;
	}
	bool dead = false;
	if (ActorInWorld(s.freed, dead)) {
		char text[160];
		std::snprintf(text, sizeof(text), "%08X moved %.1f units in the %.0f s after it was let go", s.freed,
		              static_cast<double>(Apart(PositionOf(s.freed), s.freedAt)), static_cast<double>(kFreedWatchSeconds));
		Say(s, text);
	}
	s.freed = 0;
	s.freedSeconds = 0.0f;
}

// Standing on the blade they do not turn either: the heading the engine gave
// them this frame put back to the one at the stab (the pin is in the bone's
// frame, so a turn would turn the blade out of the hand).
void HoldHeading(Stuck& s) {
	float* h = HeadingOf(s.state.actor);
	if (h == nullptr || s.state.mode != StickMode::Standing) {
		return;
	}
	float d = *h - s.startHeading;
	d = d < 0.0f ? -d : d;
	s.turned = d > s.turned ? d : s.turned;
	*h = s.startHeading;
}

// Each second while a fighter is held: how far they moved since the stab.
void SayHeld(Stuck& s, float dt) {
	s.logSeconds += dt;
	if (s.logSeconds < 1.0f) {
		return;
	}
	s.logSeconds = 0.0f;
	char text[200];
	std::snprintf(text, sizeof(text),
	              "%08X %s, %.1f units in, moved %.1f units since the stab (%.0f s), turned back from at most %.1f "
	              "degrees",
	              s.state.actor, ModeName(s.state.mode), static_cast<double>(s.state.depth),
	              static_cast<double>(Apart(PositionOf(s.state.actor), s.startAt)), static_cast<double>(s.seconds),
	              static_cast<double>(s.turned / math::kDegreesToRadians));
	Say(s, text);
}

void StepStuckBlade(BladeContactVerdict& v, const BladeContactFrame& f, const BladeEntryNow& n) {
	Stuck& s = S();
	s.stepped = true;
	WatchFreed(s, f.dtSeconds > 0.0f && f.dtSeconds < 1.0f ? f.dtSeconds : 0.0f);
	const BladeStickSettings& set = GetConfig().hands.bladeEntry.stick;
	if (s.state.mode == StickMode::None) {
		if (n.verdict.fresh) {
			Start(s, set, v, f, n);
		}
		return;
	}
	bool dead = false;
	const UInt32 actor = s.state.actor;
	const bool there = CurrentWorld() == s.state.world && ActorInWorld(actor, dead);
	NoteDeath(s);
	if (there) {
		HoldHeading(s);
	}
	BladeStickFrame frame;
	frame.anchor = there ? AnchorOf(s.part.node) : StickAnchor{};
	frame.wanted = f.wanted;
	frame.span = f.span;
	frame.worldNow = CurrentWorld();
	frame.enabled = set.enabled;
	frame.dtSeconds = f.dtSeconds;
	const BladeStickStep r = StepBladeStick(s.state, set, frame);
	if (r.end != StickEnd::None) {
		End(s, r.end, actor, &r);
		return;
	}
	const float dt = f.dtSeconds > 0.0f && f.dtSeconds < 1.0f ? f.dtSeconds : 0.0f;
	s.seconds += dt;
	s.deepest = s.state.depth > s.deepest ? s.state.depth : s.deepest;
	SayHeld(s, dt);
	if (r.posed) {
		v.pose = r.pose;
	} else if (r.drive) {
		Hang(s, r, dt);
	}
}

void WatchStuckBlade() {
	Stuck& s = S();
	if (s.state.mode != StickMode::None && !s.stepped) {
		End(s, StickEnd::Lost, s.state.actor);
	}
	s.stepped = false;
}

}  // namespace obvr::game
