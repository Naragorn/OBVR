#include "game/BlockCone.h"

#include "camera/FrameLogic.h"
#include "core/AroundCall.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/BladeBodies.h"
#include "game/GameAddresses.h"
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
	default: return "no guard";
	}
}

// The angle rule's say on a blow the engine would block: the attacker's
// blade read now, the guard as the frame left it. True keeps the block.
bool AngleKeepsBlock(void* attacker) {
	if (!g_angle.enabled || g_guard.with == BlockWith::None) {
		return true;
	}
	BlockBlow blow;
	const bool placed = ReadBlockBlow(reinterpret_cast<UInt32>(attacker), blow);
	const BlockAngleVerdict v = BlockAngleFor(g_angle, g_guard, blow);
	if (g_angleLinesLeft > 0) {
		--g_angleLinesLeft;
		OBVR_LOG("Block angle: a blow from %08X at the %s - %s%s, %.0f degrees (%s %.0f) - %s",
		         reinterpret_cast<UInt32>(attacker), BlockWithName(g_guard.with), v.why,
		         placed ? (blow.bladeValid ? ", their blade read" : ", no blade on them") : ", not an actor",
		         static_cast<double>(v.degrees),
		         g_guard.with == BlockWith::Shield ? "at most" : "at least",
		         static_cast<double>(g_guard.with == BlockWith::Shield
		                                 ? BlockSlackDegrees(g_angle.shieldDegrees, BlockAngleSettings{}.shieldDegrees)
		                                 : 90.0f - BlockSlackDegrees(g_angle.bladeDegrees, BlockAngleSettings{}.bladeDegrees)),
		         v.verdict == BlockAngle::Blocked ? "blocked"
		         : v.verdict == BlockAngle::Through ? "THROUGH, the guard stands wrong"
		                                            : "left to the game");
	}
	return v.verdict != BlockAngle::Through;
}

using ConeFn = bool(__cdecl*)(void* target, void* attacker, float* angleOut);

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
	if (!BlockConeUsesGaze(g_enabled, reinterpret_cast<UInt32>(target), Player(), g_haveGaze)) {
		const bool verdict = original(target, attacker, angleOut);
		return verdict && (!isPlayer || AngleKeepsBlock(attacker));
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
	return verdict && AngleKeepsBlock(attacker);
}

}  // namespace

void InstallBlockCone() {
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
}

}  // namespace obvr::game
