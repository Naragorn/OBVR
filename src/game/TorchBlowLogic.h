#pragma once

// The torch's blow (Nadi, 2026-10-26: "Mit torch ist es aktuell zu einfach
// gegner zu boden zu hauen ... fackel hit zwar npcs recoiled aber nicht
// gleich umhaut. umhauen erst mit ordentlich schwung whl soviel wie der
// schwerer angriff hat beim fighten"). The other hand with the torch in it
// strikes as the off hand does with a weapon drawn (GrappleLogic.h,
// OffHandStrikeFor): towards someone from ShoveSpeed a stagger, from
// ShoveHardSpeed a knockdown - the hand's speed alone, 2.4 m/s in his INI,
// an easy flick of the torch (his log of 2026-10-25: "the left hand at 2.4
// m/s towards 184D904C - hard (the off hand, a weapon drawn)", the torch
// in that hand).
//
// With [Hands] TorchKnockdownPowerOnly the speed still decides whether the
// blow lands, as a stagger, and only a power swing knocks down: the other
// hand's swing as long as the weapon's power attack (vr::SwingIsPower -
// PowerSwingMetres, a thrust PowerThrustMetres). The torch meets a body
// before its swing is that long, so a blow met while the swing is not yet a
// power one is held to the swing's end and lands as what the whole swing
// was, as the weapon's strike by motion holds its bodies (MeleeHit.h,
// SettleHeldStrike).
//
// Pure; covered by torch_blow_test.

#include "core/Types.h"
#include "game/MeleeHit.h"
#include "game/ShoveLogic.h"

namespace obvr::game {

struct TorchBlow {
	ShoveKind kind = ShoveKind::None;  // what lands now
	bool hold = false;                 // held to the swing's end
};

// The blow on the frame the torch hand meets someone. `bySpeed` is the off
// hand's strike by its speed (OffHandStrikeFor); `swingActive` and
// `swingPower` the other hand's swing now, `endedPower` its verdict this
// frame a power one (the swing just over: no longer active).
inline TorchBlow TorchBlowAtContact(bool powerOnly, ShoveKind bySpeed, bool swingActive, bool swingPower,
                                    bool endedPower) {
	TorchBlow b;
	if (bySpeed == ShoveKind::None) {
		return b;
	}
	if (!powerOnly) {
		b.kind = bySpeed;
		return b;
	}
	if (swingPower || endedPower) {
		b.kind = ShoveKind::Hard;
	} else if (swingActive) {
		b.hold = true;
	} else {
		b.kind = ShoveKind::Light;
	}
	return b;
}

// A held blow: None while its swing runs short of a power attack, then a
// knockdown once it is one or ended one, a stagger once it ended otherwise.
inline ShoveKind SettleTorchBlow(UInt32 heldSerial, UInt32 currentSerial, bool swingActive, bool swingPower,
                                 bool endedPower) {
	switch (SettleHeldStrike(heldSerial, currentSerial, swingActive, swingPower, endedPower)) {
	case HeldStrike::Wait:
		return ShoveKind::None;
	case HeldStrike::Power:
		return ShoveKind::Hard;
	case HeldStrike::Light:
		break;
	}
	return ShoveKind::Light;
}

}  // namespace obvr::game
