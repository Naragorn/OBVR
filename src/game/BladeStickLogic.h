#pragma once

// A stab that sticks (docs/blade-penetration-spec.md step 5, section 7;
// Nadi, 2026-11-08: the fighter must not move while the blade is in, as in
// Blade & Sorcery).
//
// A stab that went in (BladeEntryLogic.h, kind Stab) holds the blade:
// - Standing: a living fighter. The blade is pinned at the entry point on
//   the struck bone and moves only along its own axis, eased in and out
//   (B&S penetrationHeldDamperIn/Out), from the surface to the blade's
//   length. The caller staggers them on entry and holds their legs and
//   their blows while it is in (game::HoldStill, game::SetHeldFoe - the
//   grapple's standing hold, measured since 2026-10-13).
// - Ragdoll: a corpse, a fighter killed with the blade in, or a deep stab
//   into the torso: the body hangs on the blade - the struck bone's Havok
//   body is driven so that the entry point stays on the blade (the ragdoll
//   hold's drive, game/RagdollHold.h).
// It ends when the hand pulls the blade out along its axis past the
// surface, when the hand leaves the axis too far or turns the blade too far
// (torn out), when the anchor bone no longer reads, or after a load (the
// world counter, as the cuts: DismemberLogic.h CutInWorld).
//
// The anchor is the struck bone's world transform; the entry point, the
// axis and the blade's turn are kept in its frame, so the pin follows the
// body as it staggers. Pure; blade_stick_test.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/BladeContactLogic.h"
#include "game/BodyPart.h"
#include "game/HandBodyLogic.h"
#include "game/NiMath.h"

namespace obvr::game {

struct BladeStickSettings {
	bool enabled = true;
	// The depth's easing toward where the hand wants it, a share a second:
	// pushing in, pulling out (B&S penetrationHeldDamperIn/Out: in is the
	// harder way).
	float easeInPerSecond = 6.0f;
	float easeOutPerSecond = 12.0f;
	// Torn out: the hand's tip this far off the stuck axis, or the blade
	// turned this far from it.
	float tearUnits = 14.0f;
	float tearDegrees = 40.0f;
	// Pulled out: the hand's tip this far back past the surface.
	float pullOutUnits = 1.0f;
	// A stab into the torso this fast (metres a second) takes them down
	// into their ragdoll, hanging on the blade; below 0: never.
	float ragdollSpeedMetres = 5.0f;
	// The ragdoll's tear: the entry point this far from the blade's line
	// (the body caught on something).
	float ragdollTearUnits = 30.0f;
};

enum class StickMode : UInt8 { None, Standing, Ragdoll };

enum class StickEnd : UInt8 { None, PulledOut, TornOut, Lost, Load, Off };

inline const char* StickEndName(StickEnd e) {
	switch (e) {
	case StickEnd::PulledOut: return "pulled out";
	case StickEnd::TornOut: return "torn out";
	case StickEnd::Lost: return "its bone no longer reads";
	case StickEnd::Load: return "a load";
	case StickEnd::Off: return "switched off";
	default: return "-";
	}
}

// The struck bone's world transform.
struct StickAnchor {
	bool valid = false;
	NiMatrix33 rot = NiMatrix33::Identity();
	NiPoint3 pos{0.0f, 0.0f, 0.0f};
	float scale = 1.0f;
};

// How a stab that went in starts: standing for the living, the ragdoll for
// a corpse or a fast stab into the torso; none when off or not a stab.
inline StickMode StickStartFor(const BladeStickSettings& s, bool stabWentIn, bool alive, BodyPart part,
                               float speedMetres) {
	if (!s.enabled || !stabWentIn) {
		return StickMode::None;
	}
	if (!alive) {
		return StickMode::Ragdoll;
	}
	const bool deep = part == BodyPart::Torso && s.ragdollSpeedMetres >= 0.0f && speedMetres >= s.ragdollSpeedMetres;
	return deep ? StickMode::Ragdoll : StickMode::Standing;
}

struct BladeStickState {
	StickMode mode = StickMode::None;
	UInt32 actor = 0;
	UInt32 world = 0;  // CurrentWorld() at the start
	NiPoint3 entryLocal{0.0f, 0.0f, 0.0f};  // in the anchor's frame, unscaled
	NiPoint3 axisStart{1.0f, 0.0f, 0.0f};  // the blade's line toward the tip at the start, in the world
	float depth = 0.0f;     // the tip past the entry point, units
	float maxDepth = 0.0f;  // the blade's length
};

inline NiPoint3 StickUnit(const NiPoint3& v, const NiPoint3& fallback) {
	const float l2 = v.LengthSquared();
	return l2 > 1e-8f ? v * (1.0f / math::Sqrt(l2)) : fallback;
}

inline float StickDot(const NiPoint3& a, const NiPoint3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

// The start: the entry point, the blade's axis and turn taken into the
// anchor's frame, the depth the tip has already gone past the entry point.
// False (state untouched) when the anchor or the span does not read.
inline bool StartBladeStick(BladeStickState& st, StickMode mode, UInt32 actor, UInt32 world, const StickAnchor& anchor,
                            const BladePose& pose, const BodySpan& span, const NiPoint3& entry) {
	if (mode == StickMode::None || !anchor.valid || !span.valid || !(anchor.scale > 0.0f)) {
		return false;
	}
	const NiPoint3 guard = BladePointAt(pose, span, 0.0f);
	const NiPoint3 tip = BladePointAt(pose, span, 1.0f);
	const NiPoint3 along = tip - guard;
	const float length = math::Sqrt(along.LengthSquared());
	if (!(length > 1e-3f)) {
		return false;
	}
	const NiPoint3 axis = along * (1.0f / length);
	const NiMatrix33 inv = InverseMatrix(anchor.rot);
	BladeStickState s;
	s.mode = mode;
	s.actor = actor;
	s.world = world;
	s.entryLocal = inv * (entry - anchor.pos) * (1.0f / anchor.scale);
	s.axisStart = axis;
	s.maxDepth = length;
	const float d = StickDot(tip - entry, axis);
	s.depth = d < 0.0f ? 0.0f : (d > length ? length : d);
	st = s;
	return true;
}

struct BladeStickFrame {
	StickAnchor anchor;
	BladePose wanted;  // where the hand puts the blade
	BodySpan span;
	UInt32 worldNow = 0;
	bool enabled = true;
	float dtSeconds = 0.0f;
};

struct BladeStickStep {
	StickMode mode = StickMode::None;  // after this step
	StickEnd end = StickEnd::None;     // why it ended this step
	bool posed = false;                // standing: the blade's pose pinned
	BladePose pose;
	bool drive = false;  // ragdoll: drive the struck body by `shift`
	NiPoint3 shift{0.0f, 0.0f, 0.0f};  // where the entry point should go, less where it is
	NiPoint3 entry{0.0f, 0.0f, 0.0f};  // the entry point in the world
	float lateralUnits = 0.0f;
	float turnCos = 1.0f;  // the hand's blade against the stuck axis
	float wantedDepth = 0.0f;
};

inline StickEnd StickEndFor(const BladeStickState& st, const BladeStickFrame& f) {
	if (!f.enabled) {
		return StickEnd::Off;
	}
	if (st.world == 0 || st.world != f.worldNow) {
		return StickEnd::Load;
	}
	if (!f.anchor.valid || !f.span.valid || !(f.anchor.scale > 0.0f)) {
		return StickEnd::Lost;
	}
	return StickEnd::None;
}

// Once a frame while it sticks. A death while standing is the caller's:
// it turns the mode to Ragdoll (BecomeRagdoll).
// The entry point follows its bone (the body staggers, takes its stance);
// the blade keeps the hand's turn and is held on the line through the entry
// point, so the body moving under it does not tear it out - only the hand
// leaving the line, or turning the blade from where it went in, does.
inline BladeStickStep StepBladeStick(BladeStickState& st, const BladeStickSettings& s, const BladeStickFrame& f) {
	BladeStickStep out;
	if (st.mode == StickMode::None) {
		return out;
	}
	out.end = StickEndFor(st, f);
	NiPoint3 wantedAxis = st.axisStart;
	NiPoint3 off{0.0f, 0.0f, 0.0f};
	if (out.end == StickEnd::None) {
		out.entry = f.anchor.pos + f.anchor.rot * (st.entryLocal * f.anchor.scale);
		const NiPoint3 wantedTip = BladePointAt(f.wanted, f.span, 1.0f);
		wantedAxis = StickUnit(wantedTip - BladePointAt(f.wanted, f.span, 0.0f), st.axisStart);
		const NiPoint3 fromEntry = wantedTip - out.entry;
		out.wantedDepth = StickDot(fromEntry, wantedAxis);
		off = fromEntry - wantedAxis * out.wantedDepth;
		out.lateralUnits = math::Sqrt(off.LengthSquared());
		out.turnCos = StickDot(st.axisStart, wantedAxis);
		const float tearCos = math::Cos(s.tearDegrees * math::kDegreesToRadians);
		const float tear = st.mode == StickMode::Standing ? s.tearUnits : s.ragdollTearUnits;
		if (out.wantedDepth < -s.pullOutUnits) {
			out.end = StickEnd::PulledOut;
		} else if (out.lateralUnits > tear || (st.mode == StickMode::Standing && out.turnCos < tearCos)) {
			out.end = StickEnd::TornOut;
		}
	}
	if (out.end != StickEnd::None) {
		st = BladeStickState{};
		return out;
	}
	const float target = out.wantedDepth < 0.0f ? 0.0f : (out.wantedDepth > st.maxDepth ? st.maxDepth : out.wantedDepth);
	if (st.mode == StickMode::Standing) {
		const float rate = target > st.depth ? s.easeInPerSecond : s.easeOutPerSecond;
		const float dt = f.dtSeconds > 0.0f && f.dtSeconds < 1.0f ? f.dtSeconds : 0.0f;
		const float k = rate * dt >= 1.0f ? 1.0f : (rate * dt > 0.0f ? rate * dt : 0.0f);
		st.depth += (target - st.depth) * k;
		out.pose.rot = f.wanted.rot;
		out.pose.pos = out.entry + wantedAxis * st.depth - out.pose.rot * f.span.b;
		out.posed = true;
	} else {
		// The body hangs on the hand's blade: the entry point brought onto the
		// blade's line, sideways only - pushed on, the blade goes deeper;
		// drawn back, it comes out of the body.
		st.depth = target;
		out.shift = off;
		out.drive = true;
	}
	out.mode = st.mode;
	return out;
}

// A fighter killed with the blade in, or one the caller took down: the body
// now hangs on the blade.
inline void BecomeRagdoll(BladeStickState& st) {
	if (st.mode == StickMode::Standing) {
		st.mode = StickMode::Ragdoll;
	}
}

}  // namespace obvr::game
