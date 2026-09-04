#include "game/WorldPickHook.h"

#include "camera/FrameLogic.h"
#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "game/WorldPickTrampoline.h"

namespace obvr::game {
namespace {

NiPoint3 g_direction{};
bool g_gazeValid = false;
bool g_installed = false;
bool g_reportedUse = false;

constexpr UInt32 kTrampolineSize = 64;

bool IsThirdPerson() {
	const auto* const player = *reinterpret_cast<const UInt8* const*>(addr::kPlayerPointer);
	const UInt32 address = reinterpret_cast<UInt32>(player);
	return mem::LooksLikeObjectAddress(address) &&
	       player[addr::kPlayerIsThirdPersonOffset] != 0;
}

bool Sane(float value) {
	constexpr float kLimit = 1.0e7f;
	return value > -kLimit && value < kLimit;
}

bool SaneDirection(const NiPoint3& direction) {
	if (!Sane(direction.x) || !Sane(direction.y) || !Sane(direction.z)) {
		return false;
	}
	const float lengthSquared = direction.x * direction.x + direction.y * direction.y +
	                            direction.z * direction.z;
	return lengthSquared > 0.25f && lengthSquared < 2.25f;
}

}  // namespace

void SetWorldPickDirection(const NiPoint3& direction, bool valid) {
	g_direction = direction;
	g_gazeValid = valid && SaneDirection(direction);
}

// `stack` is the ESP that Oblivion had at 0058080C. These offsets are the
// values proved by the surrounding instructions documented in GameAddresses.
extern "C" void __cdecl OBVR_ReplaceWorldPickRay(UInt8* stack) {
	if (stack == nullptr) {
		return;
	}
	const camera::WorldPickOverride policy =
		camera::WorldPickOverrideWanted(IsThirdPerson(), g_gazeValid, g_installed);
	if (!policy.replaceDirection) {
		return;
	}

	auto write = [stack](UInt32 offset, float value) {
		*reinterpret_cast<float*>(stack + offset) = value;
	};
	// Deliberately leave esp+20/+24/+28 untouched. Those are Oblivion's player
	// activation origin; replacing them with the chase camera behind the actor
	// makes the ray collide with the player's own body before reaching a chest.
	write(0x34, g_direction.x);
	write(0x38, g_direction.y);
	write(0x3C, g_direction.z);

	if (!g_reportedUse) {
		g_reportedUse = true;
		OBVR_LOG("Crosshair target: third-person Activate and HUDInfo use the HMD direction "
		         "from Oblivion's player-safe pick origin");
	}
}

bool InstallWorldPickHook() {
	if (!mem::Verify(addr::kHookWorldPickRay, kWorldPickOriginalBytes,
	                 addr::kHookWorldPickRayPatchSize)) {
		OBVR_LOG("Crosshair target: bytes at %08X do not match the world-pick ray site; "
		         "third-person activation stays vanilla", addr::kHookWorldPickRay);
		mem::ReportForeignCode("Crosshair target", addr::kHookWorldPickRay);
		return false;
	}

	auto* const trampoline = static_cast<UInt8*>(mem::AllocExecutable(kTrampolineSize));
	if (trampoline == nullptr) {
		OBVR_LOG("Crosshair target: no executable memory for the world-pick trampoline");
		return false;
	}
	const UInt32 trampolineAddress = reinterpret_cast<UInt32>(trampoline);
	const UInt32 trampolineSize = BuildWorldPickTrampoline(
		trampoline, kTrampolineSize, trampolineAddress,
		reinterpret_cast<UInt32>(&OBVR_ReplaceWorldPickRay));
	if (trampolineSize == 0) {
		OBVR_LOG("Crosshair target: world-pick trampoline did not fit");
		return false;
	}

	UInt8 patch[addr::kHookWorldPickRayPatchSize]{};
	const UInt32 patchSize = BuildWorldPickPatch(
		patch, sizeof(patch), addr::kHookWorldPickRay, trampolineAddress);
	if (patchSize != sizeof(patch) ||
	    !mem::SafeWrite(addr::kHookWorldPickRay, patch, patchSize)) {
		OBVR_LOG("Crosshair target: failed to patch the world-pick ray at %08X",
		         addr::kHookWorldPickRay);
		return false;
	}

	g_installed = true;
	OBVR_LOG("Crosshair target: HMD world-pick hook installed at %08X, trampoline %08X (%u bytes)",
	         addr::kHookWorldPickRay, trampolineAddress, trampolineSize);
	return true;
}

}  // namespace obvr::game
