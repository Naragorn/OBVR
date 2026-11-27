#include "game/BlockCone.h"

#include <cstdio>
#include <cstring>

#include "camera/FrameLogic.h"
#include "core/AroundCall.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/BladeBodies.h"
#include "game/GameAddresses.h"
#include "game/ArrowShield.h"
#include "game/Parry.h"
#include "game/PlayerAim.h"

namespace obvr::game {
namespace {

bool g_enabled = true;
bool g_haveGaze = false;
float g_headYaw = 0.0f;
UInt32 g_linesLeft = 12;
BlockGuard g_guard;
BlockAngleSettings g_angle;
UInt32 g_angleLinesLeft = 30;

const char* BlockWithName(BlockWith with) {
	switch (with) {
	case BlockWith::Shield: return "shield";
	case BlockWith::Weapon: return "weapon";
	case BlockWith::Fists: return "fists";
	case BlockWith::Bow: return "bow";
	case BlockWith::Torch: return "torch";
	default: return "no guard";
	}
}

// The angle rule's verdict on a blow: the attacker's blade read now, the
// guard as the frame left it, judged as `with`; logged. `byContact`: the shield's and the weapon's own test
// under its forced block (ParryLogic.h, ShieldConeFor) - asked whether or
// not BlockByAngle is on, since nothing else decides that blow.
BlockAngleVerdict JudgeBlow(void* attacker, bool byContact, BlockWith with) {
	BlockGuard guard = g_guard;
	guard.with = with;
	// The torch: its shaft in the world this frame stands for the blade.
	if (with == BlockWith::Torch) {
		guard.bladeValid = guard.torchValid;
		guard.bladeA = guard.torchA;
		guard.bladeB = guard.torchB;
	}
	BlockAngleSettings settings = g_angle;
	settings.enabled = settings.enabled || byContact;
	BlockBlow blow;
	const bool placed = ReadBlockBlow(reinterpret_cast<UInt32>(attacker), blow);
	const BlockAngleVerdict v = BlockAngleFor(settings, guard, blow);
	if (g_angleLinesLeft > 0) {
		--g_angleLinesLeft;
		// The shield by contact: its own reach, and what decided (ShieldByContactVerdict).
		const bool contactShield = v.test != nullptr && v.test[0] != 0;
		const float reach = contactShield ? BlockReachUnits(g_angle.shieldReachUnits, BlockAngleSettings{}.shieldReachUnits)
		                                  : BlockReachUnits(g_angle.reachUnits, BlockAngleSettings{}.reachUnits);
		char reachText[32];
		if (reach > 0.0f) {
			std::snprintf(reachText, sizeof(reachText), "at most %.0f", static_cast<double>(reach));
		} else {
			std::snprintf(reachText, sizeof(reachText), "not asked");
		}
		char interposeText[128] = "";
		if (contactShield) {
			std::snprintf(interposeText, sizeof(interposeText), ", by %s", v.test);
			if (v.betweenUnits >= 0.0f) {
				const UInt32 at = static_cast<UInt32>(std::strlen(interposeText));
				std::snprintf(interposeText + at, sizeof(interposeText) - at,
				              " (the line to the chest %.0f off the shield, at most %.0f)",
				              static_cast<double>(v.betweenUnits), static_cast<double>(g_angle.shieldInterposeUnits));
			}
		} else if (v.interposeUnits >= 0.0f) {
			std::snprintf(interposeText, sizeof(interposeText), ", the blade %.0f off their line (at most %.0f)",
			              static_cast<double>(v.interposeUnits), static_cast<double>(g_angle.interposeUnits));
		}
		OBVR_LOG("Block angle: a blow from %08X at the %s%s - %s%s, %.0f degrees (%s %.0f), %.0f units off (%s)%s - %s",
		         reinterpret_cast<UInt32>(attacker), BlockWithName(guard.with),
		         !byContact                        ? ""
		         : guard.with == BlockWith::Shield ? " by contact (no block held, the block forced for the shield)"
		         : guard.with == BlockWith::Torch  ? " by contact (no block held, the block forced for the torch)"
		                                           : " by contact (no block held, the block forced for the weapon)",
		         v.why,
		         placed ? (blow.bladeValid ? ", their blade read" : ", no blade on them") : ", not an actor",
		         static_cast<double>(v.degrees),
		         guard.with == BlockWith::Shield ? "at most" : "at least",
		         static_cast<double>(guard.with == BlockWith::Shield
		                                 ? BlockSlackDegrees(g_angle.shieldDegrees, BlockAngleSettings{}.shieldDegrees)
		                             : v.interposeUnits >= 0.0f
		                                 ? 90.0f - BlockSlackDegrees(g_angle.bodyDegrees, BlockAngleSettings{}.bodyDegrees)
		                                 : 90.0f - BlockSlackDegrees(g_angle.bladeDegrees, BlockAngleSettings{}.bladeDegrees)),
		         static_cast<double>(v.units), reachText, interposeText,
		         v.verdict == BlockAngle::Blocked ? "blocked"
		         : v.verdict == BlockAngle::Through
		             ? (v.across ? "THROUGH, the blow passes the guard" : "THROUGH, the guard stands wrong")
		             : "left to the game");
	}
	return v;
}

// The angle rule's say on a blow the engine would block. True keeps the block.
bool AngleKeepsBlock(void* attacker) {
	if (!g_angle.enabled || g_guard.with == BlockWith::None) {
		return true;
	}
	return JudgeBlow(attacker, false, g_guard.with).verdict != BlockAngle::Through;
}

// A blow under a block forced by contact (game/Parry.h, ShieldForcedBlow,
// WeaponForcedBlow, TorchForcedBlow): the shield's verdict, then the
// weapon's and the torch's (not while their hand swings), decide
// (ParryLogic.h, ContactConeFor); Original leaves it to the game's cone
// below. An arrow judged at its impact (game/ArrowShield.h) takes that
// verdict for the shield, the one its impact showed.
ShieldCone ContactConeAt(void* attacker, bool& torchFalls) {
	torchFalls = false;
	const bool shieldForced = ShieldForcedBlow();
	const bool weaponForced = WeaponForcedBlow();
	const bool torchForced = TorchForcedBlow();
	if (!shieldForced && !weaponForced && !torchForced) {
		return ShieldCone::Original;
	}
	ContactVerdicts c;
	c.arrow = TakeArrowVerdict(reinterpret_cast<UInt32>(attacker));
	c.shieldAsked = shieldForced && g_guard.shieldPose.byContact;
	c.shieldRaised = g_guard.shieldPose.raised;
	if (c.shieldAsked && c.arrow == KeptArrow::None) {
		c.shield = JudgeBlow(attacker, true, BlockWith::Shield).verdict;
	}
	c.weaponAsked = weaponForced && g_guard.weaponPose.byContact && !g_guard.weaponPose.swinging;
	c.weaponRaised = g_guard.weaponPose.raised;
	if (c.weaponAsked && c.shield != BlockAngle::Blocked && c.arrow != KeptArrow::Blocked) {
		c.weapon = JudgeBlow(attacker, true, BlockWith::Weapon).verdict;
	}
	c.torchAsked = torchForced && g_guard.torchPose.byContact && !g_guard.torchPose.swinging;
	c.torchRaised = g_guard.torchPose.raised;
	if (c.torchAsked && c.shield != BlockAngle::Blocked && c.weapon != BlockAngle::Blocked) {
		c.torch = JudgeBlow(attacker, true, BlockWith::Torch).verdict;
		if (c.torch == BlockAngle::Blocked) {
			// With nothing readied the hit handler's hand-to-hand branch would
			// block none of it against an armed attacker: marked, the engine's
			// own block share is taken (game/Parry.h, NoteTorchBlock).
			NoteTorchBlock();
		}
	}
	torchFalls = TorchFallsBack(c);
	return ContactConeFor(true, true, c);
}

using ConeFn = bool(__cdecl*)(void* target, void* attacker, float* angleOut);

// The raised torch blocked the player's blow: the hit handler takes the
// engine's block share for it (game/Parry.h NoteTorchBlock).
UInt32 g_torchBlockLines = 12;
void NoteIfTorch(bool blocked, void* attacker, bool torchFalls = false) {
	if (!blocked || (g_guard.with != BlockWith::Torch && !torchFalls)) {
		return;
	}
	NoteTorchBlock();
	if (g_torchBlockLines > 0) {
		--g_torchBlockLines;
		OBVR_LOG("Block: %08X's blow met the raised torch - blocked by the Block skill's share", reinterpret_cast<UInt32>(attacker));
	}
}

UInt32 Player() { return *reinterpret_cast<const UInt32*>(addr::kPlayerPointer); }

bool __cdecl OnBlockCone(void* target, void* attacker, float* angleOut) {
	const auto original = reinterpret_cast<ConeFn>(kBlockConeCheck);
	// A parried blow is blocked whichever way the player faces; another
	// attacker's under a block forced for a parry is not (game/Parry.h).
	const ParryCone parry = ParryConeFor(reinterpret_cast<UInt32>(target), reinterpret_cast<UInt32>(attacker));
	if (parry == ParryCone::Blocked) {
		if (angleOut != nullptr) {
			*angleOut = 0.0f;
		}
		return true;
	}
	if (parry == ParryCone::NotBlocked) {
		return false;
	}
	const bool isPlayer = Player() != 0 && reinterpret_cast<UInt32>(target) == Player();
	// The shield by contact: in the blow's way, blocked whichever way the
	// player looks; not, it lands (game/Parry.h, ShieldForcedBlow).
	bool torchFalls = false;  // the torch by contact unjudged, raised: the game's cone stands in
	const ShieldCone shield = isPlayer ? ContactConeAt(attacker, torchFalls) : ShieldCone::Original;
	if (shield != ShieldCone::Original) {
		if (shield == ShieldCone::Blocked && angleOut != nullptr) {
			*angleOut = 0.0f;
		}
		return shield == ShieldCone::Blocked;
	}
	if (!BlockConeUsesGaze(g_enabled, reinterpret_cast<UInt32>(target), Player(), g_haveGaze)) {
		const bool verdict = original(target, attacker, angleOut);
		const bool blocked = verdict && (!isPlayer || AngleKeepsBlock(attacker));
		if (isPlayer) {
			NoteIfTorch(blocked, attacker, torchFalls);
		}
		return blocked;
	}
	PlayerRotation saved{};
	if (!ReadPlayerRotation(saved)) {
		return original(target, attacker, angleOut);
	}
	const float gazeYaw = camera::PlayerYawForGaze(saved.yaw, g_headYaw);
	float bodyAngle = 0.0f;
	const bool bodyVerdict = original(target, attacker, &bodyAngle);
	WritePlayerYaw(gazeYaw);
	float gazeAngle = 0.0f;
	const bool verdict = original(target, attacker, &gazeAngle);
	WritePlayerYaw(saved.yaw);
	if (angleOut != nullptr) {
		*angleOut = gazeAngle;
	}
	if (g_linesLeft > 0) {
		--g_linesLeft;
		OBVR_LOG("Block: a blow at the player's block - %.0f degrees off the view (%s), %.0f off the body (%s)",
		         static_cast<double>(gazeAngle), verdict ? "blocked" : "outside the cone",
		         static_cast<double>(bodyAngle), bodyVerdict ? "would have blocked" : "would have missed");
	}
	const bool blocked = verdict && AngleKeepsBlock(attacker);
	NoteIfTorch(blocked, attacker, torchFalls);
	return blocked;
}

}  // namespace

void InstallBlockCone() {
	// The arrow impact's block check with it: the shield by contact judges arrows too (game/ArrowShield.h).
	InstallArrowShield();
	const UInt32 displacement = mem::CallRelativeDisplacement(kCallBlockConeCheck, kBlockConeCheck);
	const UInt8 expected[5] = {0xE8, static_cast<UInt8>(displacement & 0xFF),
	                           static_cast<UInt8>((displacement >> 8) & 0xFF),
	                           static_cast<UInt8>((displacement >> 16) & 0xFF),
	                           static_cast<UInt8>((displacement >> 24) & 0xFF)};
	if (!mem::Verify(kCallBlockConeCheck, expected, sizeof(expected))) {
		OBVR_LOG("Block: the call at %08X is not `call %08X` - the block cone stays the body's",
		         kCallBlockConeCheck, kBlockConeCheck);
		mem::ReportForeignCode("Block", kCallBlockConeCheck);
		return;
	}
	UInt8 patch[5];
	if (mem::BuildCallSitePatch(patch, sizeof(patch), kCallBlockConeCheck,
	                            reinterpret_cast<UInt32>(&OnBlockCone)) != sizeof(patch) ||
	    !mem::SafeWrite(kCallBlockConeCheck, patch, sizeof(patch))) {
		OBVR_LOG("Block: could not reroute the block cone at %08X", kCallBlockConeCheck);
		return;
	}
	OBVR_LOG("Block: the block cone rerouted at %08X - the player's looks along the view", kCallBlockConeCheck);
}

void SetBlockCone(bool enabled, bool haveGaze, float headYaw) {
	g_enabled = enabled;
	g_haveGaze = haveGaze;
	g_headYaw = headYaw;
}

void SetBlockGuard(const BlockGuard& guard, const BlockAngleSettings& settings) {
	g_guard = guard;
	g_angle = settings;
	SetShieldGuard(guard.shieldPose.byContact);
	SetWeaponGuard(guard.weaponPose.byContact && !guard.weaponPose.swinging);
	SetTorchContactGuard(guard.torchPose.byContact && !guard.torchPose.swinging);
	SetArrowShieldGuard(guard);
}

}  // namespace obvr::game
