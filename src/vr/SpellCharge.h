#pragma once

// Charged spells (Nadi, 2026-11-20, controls-spec "spells charged and released"): a short press of
// the casting hand's trigger casts as before; held, the spell stays charged in the hand and goes when
// the trigger lets go, in stages by the time held. Pure: the cast control sent to the engine, whether
// its release is held back, and when the held release is made or dropped.
//
// Read and measured (game/SpellRelease.h, spell-hold-probe 2026-11-22): the engine releases a
// spell 44 frames (0.61 s at 72 Hz) after the press, at one call in its animation update, and with
// the control held it casts again and again (a second spell at frame 140). So the control goes to
// the engine as a short pulse at the press, never held, and the release is held back for as long as
// the trigger is held past a tap.

namespace obvr::vr {

struct SpellChargeSettings {
	bool enabled = true;           // [Hands] SpellCharge
	float tapSeconds = 0.25f;      // [Hands] SpellChargeTapSeconds - held longer, it charges
	float stage2Seconds = 0.75f;   // [Hands] SpellChargeStage2Seconds
	float stage3Seconds = 1.5f;    // [Hands] SpellChargeStage3Seconds
};

// The pulse the cast control is held for at a press: long enough for the engine to read a press
// (a frame at 30 fps and some), short of its second cast.
inline constexpr float kSpellCastPulseSeconds = 0.1f;

struct SpellChargeInput {
	bool triggerHeld = false;  // the casting hand's cast control, as the hand planned it
	bool menuUp = false;
	float dtSeconds = 0.0f;
};

struct SpellChargeState {
	bool held = false;          // the trigger held last frame
	float heldSeconds = 0.0f;   // since the press
	float pulseLeft = 0.0f;     // the cast control's pulse still to send
	bool charging = false;      // held past the tap: the release is held back
	bool dropped = false;       // this press's charge was dropped: no new charge before the next press
	int stage = 0;
};

struct SpellChargeStep {
	bool castControl = false;   // what the engine's cast control gets this frame
	bool holdRelease = false;   // hold back the engine's release this frame
	bool fireRelease = false;   // make the held release now (the trigger let go)
	bool dropRelease = false;   // drop a held release unmade (a menu came up while charging)
	bool pressed = false;       // a press this frame
	bool chargeBegan = false;   // the press became a charge this frame
	int stage = 0;              // 1-3 while charging and at the release; 0 otherwise
	float heldSeconds = 0.0f;
};

inline int SpellChargeStage(float heldSeconds, const SpellChargeSettings& s) {
	return heldSeconds >= s.stage3Seconds ? 3 : heldSeconds >= s.stage2Seconds ? 2 : 1;
}

inline SpellChargeStep StepSpellCharge(SpellChargeState& st, const SpellChargeInput& in, const SpellChargeSettings& s) {
	SpellChargeStep out;
	const float dt = in.dtSeconds > 0.0f ? in.dtSeconds : 0.0f;
	if (!s.enabled) {
		// As vanilla: the control as held, nothing held back; a charge under way is let go.
		out.castControl = in.triggerHeld;
		out.fireRelease = st.charging;
		st = SpellChargeState{};
		st.held = in.triggerHeld;
		return out;
	}
	const bool press = in.triggerHeld && !st.held;
	if (press) {
		if (st.charging) {
			out.fireRelease = true;  // a release still held from a press not seen let go
		}
		st.heldSeconds = 0.0f;
		st.pulseLeft = kSpellCastPulseSeconds;
		st.charging = false;
		st.dropped = false;
		st.stage = 0;
		out.pressed = true;
	} else if (in.triggerHeld) {
		st.heldSeconds += dt;
	}
	out.castControl = st.pulseLeft > 0.0f;
	st.pulseLeft = st.pulseLeft > dt ? st.pulseLeft - dt : 0.0f;
	if (in.triggerHeld && !st.charging && !st.dropped && st.heldSeconds >= s.tapSeconds) {
		st.charging = true;
		out.chargeBegan = true;
	}
	if (st.charging) {
		st.stage = SpellChargeStage(st.heldSeconds, s);
		if (in.menuUp) {
			out.dropRelease = true;
			st.charging = false;
			st.dropped = true;
		} else if (!in.triggerHeld) {
			out.fireRelease = true;
			st.charging = false;
		} else {
			out.holdRelease = true;
		}
	}
	out.stage = (out.holdRelease || out.fireRelease) ? st.stage : 0;
	out.heldSeconds = st.heldSeconds;
	st.held = in.triggerHeld;
	return out;
}

}  // namespace obvr::vr
