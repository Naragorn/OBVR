#include "game/BlockCone.h"

#include "camera/FrameLogic.h"
#include "core/AroundCall.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "game/PlayerAim.h"

namespace obvr::game {
namespace {

bool g_enabled = true;
bool g_haveGaze = false;
float g_headYaw = 0.0f;
UInt32 g_linesLeft = 12;

using ConeFn = bool(__cdecl*)(void* target, void* attacker, float* angleOut);

UInt32 Player() { return *reinterpret_cast<const UInt32*>(addr::kPlayerPointer); }

bool __cdecl OnBlockCone(void* target, void* attacker, float* angleOut) {
	const auto original = reinterpret_cast<ConeFn>(kBlockConeCheck);
	if (!BlockConeUsesGaze(g_enabled, reinterpret_cast<UInt32>(target), Player(), g_haveGaze)) {
		return original(target, attacker, angleOut);
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
	return verdict;
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

}  // namespace obvr::game
