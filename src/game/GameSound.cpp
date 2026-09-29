#include "game/GameSound.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "game/GrabPhysics.h"

namespace obvr::game {
namespace {

constexpr UInt32 kOsGlobalsPointer = 0x00B33398;
constexpr UInt32 kOsGlobalsSoundOffset = 0x24;
constexpr UInt32 kMakeSound = 0x006AE0A0;    // thiscall(system, formID, flags, 0), ret 0Ch
constexpr UInt32 kStartSound = 0x006B7190;   // thiscall(sound, 0), ret 4
constexpr UInt32 kHandOverSound = 0x006B73E0;  // thiscall(sound)
constexpr UInt32 kGameFree = 0x00401F20;     // cdecl(p)
constexpr UInt32 kPlayLanding = 0x006B1900;  // cdecl(actor, material)
constexpr UInt32 kLandingCall = 0x005FDA0F;  // mov edx,[ebx+214h]; push edx; push esi
constexpr UInt32 kLookupFormById = 0x0046B250;  // xOBSE GameAPI.cpp, cdecl, as HandBones.cpp
// The flags the script command PlaySound passes: not placed.
constexpr UInt32 kSoundFlagsUnplaced = 0x101;
constexpr UInt32 kControllerGroundMaterial = 0x214;
// xOBSE GameForms.h FormType: Sound follows Race (9).
constexpr UInt8 kFormTypeSound = 10;

struct Expected {
	UInt32 address;
	UInt8 bytes[8];
	UInt32 size;
	const char* name;
};

const Expected kExpected[] = {
	{kMakeSound, {0x6A, 0xFF, 0x68, 0x7A, 0x65, 0x9C, 0x00}, 7, "the sound maker"},
	{kStartSound, {0x8B, 0xC1, 0x8B, 0x0D, 0x14, 0xC2, 0xB3, 0x00}, 8, "the sound start"},
	{kHandOverSound, {0xA1, 0x98, 0x3A, 0xB3, 0x00, 0x85, 0xC0}, 7, "the sound hand-over"},
	{kGameFree, {0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0}, 6, "the game's free"},
	{kPlayLanding, {0xA1, 0x0C, 0xC2, 0xB3, 0x00, 0x83, 0xEC, 0x10}, 8, "the landing sound"},
	{kLandingCall, {0x8B, 0x93, 0x14, 0x02, 0x00, 0x00, 0x52, 0x56}, 8, "the landing's material"},
};

bool g_verified = false;

UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }
bool LooksLikeObject(UInt32 address) { return mem::LooksLikeObjectAddress(address); }

}  // namespace

bool VerifyGameSoundAddresses() {
	bool ok = true;
	for (const Expected& e : kExpected) {
		if (!mem::Verify(e.address, e.bytes, e.size)) {
			OBVR_LOG("Sound: %s at %08X is not the bytes read; OBVR plays no sounds", e.name, e.address);
			ok = false;
		}
	}
	g_verified = ok;
	return ok;
}

bool PlaySoundForm(UInt32 formId) {
	if (!g_verified) {
		return false;
	}
	using LookupFn = const UInt8*(__cdecl*)(UInt32 id);
	const UInt8* const form = reinterpret_cast<LookupFn>(kLookupFormById)(formId);
	if (!LooksLikeObject(reinterpret_cast<UInt32>(form)) || form[addr::kFormTypeOffset] != kFormTypeSound) {
		return false;
	}
	const UInt32 globals = Read(kOsGlobalsPointer);
	const UInt32 system = LooksLikeObject(globals) ? Read(globals + kOsGlobalsSoundOffset) : 0;
	if (!LooksLikeObject(system)) {
		return false;
	}
	using MakeFn = void*(__thiscall*)(void* system, UInt32 formId, UInt32 flags, UInt32 unused);
	using StartFn = void(__thiscall*)(void* sound, UInt32 unused);
	using HandOverFn = void(__thiscall*)(void* sound);
	using FreeFn = void(__cdecl*)(void* p);
	void* const sound =
		reinterpret_cast<MakeFn>(kMakeSound)(reinterpret_cast<void*>(system), formId, kSoundFlagsUnplaced, 0);
	if (sound == nullptr) {
		return false;
	}
	reinterpret_cast<StartFn>(kStartSound)(sound, 0);
	reinterpret_cast<HandOverFn>(kHandOverSound)(sound);
	reinterpret_cast<FreeFn>(kGameFree)(sound);
	return true;
}

bool PlayPlayerLandingSound() {
	if (!g_verified) {
		return false;
	}
	const UInt32 player = Read(addr::kPlayerPointer);
	if (!LooksLikeObject(player)) {
		return false;
	}
	using ControllerOfFn = void*(__thiscall*)(void* actor);
	const UInt32 controller = reinterpret_cast<UInt32>(
		reinterpret_cast<ControllerOfFn>(kControllerOf)(reinterpret_cast<void*>(player)));
	if (!LooksLikeObject(controller)) {
		return false;
	}
	using LandingFn = void(__cdecl*)(void* actor, UInt32 material);
	reinterpret_cast<LandingFn>(kPlayLanding)(reinterpret_cast<void*>(player),
	                                          Read(controller + kControllerGroundMaterial));
	return true;
}

}  // namespace obvr::game
