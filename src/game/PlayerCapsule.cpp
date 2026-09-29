#include "game/PlayerCapsule.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "game/GrabPhysics.h"
#include "game/PlayerCapsuleLogic.h"

namespace obvr::game {
namespace {

constexpr UInt32 kControllerFlags = 0x1F4;        // bit 0: a wide creature
constexpr UInt32 kControllerSlot = 0x36C;
constexpr UInt32 kControllerCapsule = 0x374;      // slot 0's bhk wrapper; its hkShape's radius at +0x0C
constexpr UInt32 kControllerOwnRadius = 0x3A0;       // the game's own, Havok units
constexpr UInt32 kControllerTargetRadius = 0x3A8;
constexpr UInt32 kSetRadius = 0x00894BD0;         // thiscall(controller, float), ret 4
constexpr UInt8 kSetRadiusBytes[] = {0x56, 0x8B, 0xF1, 0xF6, 0x86, 0xF4, 0x01, 0x00, 0x00, 0x01};

bool LooksLikeObject(UInt32 address) { return mem::LooksLikeObjectAddress(address); }
UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }
float ReadFloat(UInt32 address) { return *reinterpret_cast<const float*>(address); }

int g_verified = -1;  // -1 not yet checked
UInt32 g_lines = 8;
// Whether OBVR has asked for a radius since the scale was last 1.
bool g_asked = false;

UInt32 PlayerController() {
	if (g_verified < 0) {
		g_verified = mem::Verify(kSetRadius, kSetRadiusBytes, sizeof(kSetRadiusBytes)) ? 1 : 0;
		if (g_verified == 0) {
			OBVR_LOG("Body: SetSize's radius setter at %08X is not the bytes read - the body's radius is left alone",
			         kSetRadius);
		}
	}
	if (g_verified != 1) {
		return 0;
	}
	const UInt32 player = Read(addr::kPlayerPointer);
	if (!LooksLikeObject(player)) {
		return 0;
	}
	using ControllerOfFn = void*(__thiscall*)(void* actor);
	const UInt32 controller =
		reinterpret_cast<UInt32>(reinterpret_cast<ControllerOfFn>(kControllerOf)(reinterpret_cast<void*>(player)));
	return LooksLikeObject(controller) ? controller : 0;
}

}  // namespace

PlayerBodyReading ReadPlayerBody() {
	PlayerBodyReading r;
	const UInt32 controller = PlayerController();
	if (controller == 0) {
		return r;
	}
	r.valid = true;
	r.gameRadiusUnits = ReadFloat(controller + kControllerOwnRadius) / kHavokPerUnit;
	r.targetRadiusUnits = ReadFloat(controller + kControllerTargetRadius) / kHavokPerUnit;
	r.slot = Read(controller + kControllerSlot);
	const UInt32 capsule = Read(controller + kControllerCapsule);
	const UInt32 shape = LooksLikeObject(capsule) ? Read(capsule + 0x08) : 0;
	r.capsuleRadiusUnits = LooksLikeObject(shape) ? ReadFloat(shape + 0x0C) / kHavokPerUnit : 0.0f;
	return r;
}

void StepPlayerCapsule(float scale) {
	const UInt32 controller = PlayerController();
	if (controller == 0 || (Read(controller + kControllerFlags) & 1u) != 0) {
		return;
	}
	const float gameRadius = ReadFloat(controller + kControllerOwnRadius);
	const float target = ReadFloat(controller + kControllerTargetRadius);
	const float wanted = BodyRadiusTarget(gameRadius, scale);
	const bool scaled = BodyRadiusScaled(scale);
	if (!BodyRadiusRequestNeeded(target, wanted, scaled, g_asked)) {
		if (!scaled) {
			g_asked = false;
		}
		return;
	}
	g_asked = scaled;
	using SetRadiusFn = void(__thiscall*)(void* controller, float radius);
	reinterpret_cast<SetRadiusFn>(kSetRadius)(reinterpret_cast<void*>(controller), wanted);
	if (g_lines > 0) {
		--g_lines;
		OBVR_LOG("Body: the player's radius asked for %.1f units (the game's %.1f x %.2f; the target was %.1f, now "
		         "%.1f; slot %u)",
		         static_cast<double>(wanted / kHavokPerUnit), static_cast<double>(gameRadius / kHavokPerUnit),
		         static_cast<double>(ClampBodyRadiusScale(scale)), static_cast<double>(target / kHavokPerUnit),
		         static_cast<double>(ReadFloat(controller + kControllerTargetRadius) / kHavokPerUnit),
		         Read(controller + kControllerSlot));
	}
}

}  // namespace obvr::game
