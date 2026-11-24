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
	// [Hands] SpellChargeStrength: the stages make the spell stronger and dearer (Nadi's default);
	// off, a charge is look and sound only.
	bool strength = true;
};

// Strength and cost by the stage (Nadi, 2026-11-20: "100/125/150 % Wirkung für ebenso mehr
// Magicka"): stage 1 and a tap as the spell is, stage 2 125 %, stage 3 150 % - its magnitude and its
// magicka alike.
inline float SpellChargeFactor(int stage, const SpellChargeSettings& s) {
	if (!s.strength || stage <= 1) {
		return 1.0f;
	}
	return stage == 2 ? 1.25f : 1.5f;
}

// The factor the player can pay for: with `magicka` left and the spell costing `baseCost`, no more
// than magicka / baseCost, never below 1 (less than the spell itself, the engine refuses the cast as
// ever). A spell that costs nothing gets the whole stage; a cost not read (negative), none of it.
inline float AffordableChargeFactor(float wanted, float magicka, float baseCost) {
	if (wanted <= 1.0f || baseCost < 0.0f) {
		return 1.0f;
	}
	if (baseCost == 0.0f) {
		return wanted;
	}
	const float most = magicka / baseCost;
	const float f = most < wanted ? most : wanted;
	return f > 1.0f ? f : 1.0f;
}

// The magicka a charged release costs beyond what the engine took (`paid`, its own cost).
inline float ExtraChargeCost(float paid, float factor) {
	return paid > 0.0f && factor > 1.0f ? paid * (factor - 1.0f) : 0.0f;
}

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
	bool castHeld = false;      // the trigger held since its press, not dropped by a menu (a touch cast waits)
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
	out.castHeld = in.triggerHeld && !st.dropped && !in.menuUp;
	st.held = in.triggerHeld;
	return out;
}

// What is seen of a charge (Nadi, 2026-11-23: "warum sieht man an der hand nichts? das wäre eig der
// hauptgrund das zu bauen", "wieso können wir nicht den feuer ball nehmen der wegschiesst?"): the
// spell's own projectile model (its first effect's MGEF model, e.g. MagicEffects\Fireball.NIF for
// Flare) at the casting palm while the release is held back, growing from a small first size at the
// charge's start to its full size at stage 3. Gone at the let go - the spell itself flies from there
// - and when a menu drops it. The sizes are shares of the flying model's own: at 1 its flames covered
// the arm in the harness film (2026-11-23), so the full size is half of it - a guess for the headset.
inline constexpr float kSpellOrbFirstScale = 0.15f;
inline constexpr float kSpellOrbFullScale = 0.5f;

struct SpellOrbView {
	bool shown = false;
	float scale = 0.0f;  // of the model's own size
	bool full = false;   // grown to kSpellOrbFullScale
};

inline SpellOrbView SpellOrbFor(const SpellChargeStep& step, const SpellChargeSettings& s) {
	SpellOrbView v;
	if (!step.holdRelease) {
		return v;
	}
	const float span = s.stage3Seconds - s.tapSeconds;
	float t = span > 1e-4f ? (step.heldSeconds - s.tapSeconds) / span : 1.0f;
	t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
	v.shown = true;
	v.scale = kSpellOrbFirstScale + (kSpellOrbFullScale - kSpellOrbFirstScale) * t;
	v.full = t >= 1.0f;
	return v;
}

// The cast's own animation in two halves (Nadi, 2026-11-24: "die animation halbieren! die erste
// hälfte am anfang, dann wird gehalten, und erst beim loslassen des triggers kommt die 2te
// hälfte"): it plays as the engine plays it up to its release key, where the engine comes to the
// release OBVR holds back; there it stays while the spell is held, and plays on from there the frame
// the release is made or dropped. Its second half - the hand opened, filmed 2026-11-24 after the
// release key for Flare, the fire touch and the novice heal - so comes with the spell.
//
// A touch spell waits at its start instead (Nadi, 2026-11-24: "da wärs cooler am anfang und während
// des charges keine anim zu haben. und dann wenn ich loslasse die volle"): kept where it is first
// seen while the trigger is held - at its start, time 0 - so the engine does not come to its release then; let go, the whole
// animation plays and the engine releases at its own key (game/SpellRelease.h gives that release the
// charge's strength).
//
// Measured (harness 2026-11-24): the cast is the anim data's slot 1, group 0x22 for the heal
// (CastSelf), 0x23 the fire touch (CastTouch), 0x24 Flare (CastTarget); the release at its key 1.
// The Alt groups 0x25-0x27 after xOBSE's TESAnimGroup (obse/NiNodes.h), not seen in a run.
inline bool IsCastAnimGroup(unsigned code) { return code >= 0x22 && code <= 0x27; }
inline bool IsTouchCastGroup(unsigned code) { return code == 0x23 || code == 0x26; }
struct CastAnimPause {
	bool paused = false;
	float time = 0.0f;  // the sequence's time it stays at
};

// Once a frame per anim data, after the engine's animation update: with the release held back now
// (`heldBack`: neither made nor dropped yet), the cast held (SpellChargeStep::castHeld), the cast
// slot's group and the sequence's time now (its offset plus the anim data's clock). True with the
// `offset` the sequence is to get - the write the engine makes itself to put a sequence at a key
// (game/BowRelease.h); false when it is left alone.
inline bool StepCastAnimPause(CastAnimPause& p, bool heldBack, bool castHeld, unsigned groupCode, float timeNow,
                              float clock, float& offset) {
	const bool wait = IsCastAnimGroup(groupCode) && (heldBack || (castHeld && IsTouchCastGroup(groupCode)));
	if (!wait) {
		p = CastAnimPause{};
		return false;
	}
	if (!p.paused) {
		p.paused = true;
		// A touch cast waits at its start (time 0): on its first frame the sequence still reads its last
		// play's time (14.2 s measured, 2026-11-24), and kept there the engine ends the cast unreleased.
		p.time = heldBack ? timeNow : 0.0f;
	}
	offset = p.time - clock;
	return true;
}

}  // namespace obvr::vr
