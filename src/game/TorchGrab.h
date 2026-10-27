#pragma once

// A torch lying in the world, taken at its grip as a weapon is (Nadi,
// 2026-10-27: "Fackeln sollen wie waffen aus der welt aufgreifbar am griff
// wo sie dann auch gehalten werden, kreis und hand tooltip bekommen und dann
// dort aufgenommen werden können per bücken und greifen oder teleport. Und
// dann immer aktiv mit flamme"; free-weapons-spec section 5.10). The torch is
// the other hand's: the engine has one torch slot and shows it in the left
// hand only (section 2.3), so the other hand takes it - at its grip, or
// pulled in - and the weapon hand carries it as any object, as the other
// hand does a weapon (decision 8 mirrored). Taken into the inventory by the
// game's activation (its owner's rules), equipped with EquipItem; a torch in
// the hand burns as the engine always lights it there (0x00478270).
//
// The grip is where the palm sits on the torch in the hand: 2.7 units below
// the model's origin along its +y, measured (torch-grab.txt 2026-10-27: the
// palm at -0.8 -2.7 -1.4 on the held torch's model, the "Torch grab: in the
// hand" line). Torch02.nif's shaft runs along +y from -14.4 to 33.5 with the
// flame node at 27 and AttachLight at 35 (pyffi, 2026-10-27).
//
// Pure, covered by torch_grab_test; CameraHook reads the world and sends
// the calls.

#include "core/Types.h"

namespace obvr::game {

inline constexpr float kGripAxialTorch = -2.7f;

inline constexpr UInt8 kFormTypeLightCode = 0x1A;
inline constexpr UInt32 kLightCarriedFlag = 0x02;

// A light the hand can carry - a torch - and not a lamp of the cell.
inline bool IsTorchLight(UInt8 formType, UInt32 lightFlags) {
	return formType == kFormTypeLightCode && (lightFlags & kLightCarriedFlag) != 0;
}

// Whether the hand nearest the thing is the one that takes it: the weapon
// hand a weapon, the other hand a torch.
inline bool IsTakingHand(bool torch, bool nearLeft) { return torch ? nearLeft : !nearLeft; }

// Why the other hand does not take a torch it is at, or Takes.
enum class TorchTakeGate : UInt8 {
	Takes,
	Off,          // [Hands] GrabEquipsTorches 0
	NotNow,       // a menu, no world, a fit, a grab in flight, dead
	BowDrawn,     // the bow is held in that hand: the engine hides a torch there
	HandFull,     // a shield or a torch already hangs on that hand
	Holster,      // the holster claims the grip
};

inline TorchTakeGate TorchTakeGateFor(bool enabled, bool allowed, bool bowDrawn, bool handFull,
                                      bool holsterClaims) {
	if (!enabled) {
		return TorchTakeGate::Off;
	}
	if (!allowed) {
		return TorchTakeGate::NotNow;
	}
	if (bowDrawn) {
		return TorchTakeGate::BowDrawn;
	}
	if (handFull) {
		return TorchTakeGate::HandFull;
	}
	if (holsterClaims) {
		return TorchTakeGate::Holster;
	}
	return TorchTakeGate::Takes;
}

inline const char* TorchTakeGateWords(TorchTakeGate g) {
	switch (g) {
	case TorchTakeGate::Takes:
		return "the other hand takes it";
	case TorchTakeGate::Off:
		return "GrabEquipsTorches is 0";
	case TorchTakeGate::NotNow:
		return "not now (a menu, a fit, a grab in flight)";
	case TorchTakeGate::BowDrawn:
		return "the bow holds that hand";
	case TorchTakeGate::HandFull:
		return "that hand holds a shield or a torch";
	case TorchTakeGate::Holster:
		return "the holster has the grip";
	}
	return "?";
}

// The take's steps after the activation: the equip sent once, then the wait
// for the game to show the torch worn; given up after the wait, cancelled
// when the world goes (a menu).
enum class TorchTakePhase : UInt8 { Idle, Equipping };

struct TorchTakeState {
	TorchTakePhase phase = TorchTakePhase::Idle;
	UInt32 form = 0;  // the torch's base form, read before the take (the reference goes)
	bool equipSent = false;
	float wait = 0.0f;
};

struct TorchTakeInput {
	bool allowed = false;  // in the world, no menu
	UInt32 wornTorch = 0;  // the torch form the player wears now, 0 none
	float dt = 0.0f;
};

struct TorchTakeVerdict {
	bool equipNow = false;  // send EquipItem for `form` this frame
	UInt32 form = 0;
	bool done = false;      // the game shows it worn
	bool gaveUp = false;
	bool cancelled = false;
};

inline void BeginTorchTake(TorchTakeState& s, UInt32 form, float waitSeconds) {
	s = TorchTakeState{};
	s.phase = TorchTakePhase::Equipping;
	s.form = form;
	s.wait = waitSeconds == waitSeconds && waitSeconds > 0.0f ? waitSeconds : 2.0f;
}

inline TorchTakeVerdict StepTorchTake(TorchTakeState& s, const TorchTakeInput& in) {
	TorchTakeVerdict v;
	if (s.phase == TorchTakePhase::Idle) {
		return v;
	}
	if (!in.allowed) {
		s = TorchTakeState{};
		v.cancelled = true;
		return v;
	}
	if (!s.equipSent) {
		s.equipSent = true;
		v.equipNow = true;
		v.form = s.form;
		return v;
	}
	if (in.wornTorch == s.form && s.form != 0) {
		v.done = true;
		s = TorchTakeState{};
		return v;
	}
	s.wait -= in.dt == in.dt && in.dt > 0.0f ? in.dt : 0.0f;
	if (s.wait <= 0.0f) {
		v.gaveUp = true;
		s = TorchTakeState{};
	}
	return v;
}

}  // namespace obvr::game
