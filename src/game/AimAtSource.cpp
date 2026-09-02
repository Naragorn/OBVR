#include "game/AimAtSource.h"

#include "camera/FrameLogic.h"
#include "core/AddressSpace.h"
#include "core/AroundCall.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "game/PlayerAim.h"

// The callbacks the stubs reach. cdecl and extern "C", as the cast hook's
// is, so the names the stubs are built against are the names the linker
// sees.
extern "C" void __cdecl OBVR_AimSourceBeforeKey(void* caster);
extern "C" void __cdecl OBVR_AimSourceBeforeFactory(void* caster);
extern "C" void __cdecl OBVR_AimSourceBeforeArrow(void* arrow);
extern "C" void __cdecl OBVR_AimSourceAfterKey();
extern "C" void __cdecl OBVR_AimSourceAfterFactory();
extern "C" void __cdecl OBVR_AimSourceAfterArrow();

namespace obvr::game {
namespace {

// One stub per site, each with its own slots: the return address the stub
// keeps aside and the address of the arguments it records. Static, because
// the stub's bytes carry these addresses; they live in the stub's own
// allocation, after the code.
struct Site {
	mem::AroundCallSlots slots;
	bool installed = false;
};

// The three sites. The key handler is where an NPC's attacks read the
// heading; the projectile factory is where every spell's launch rotation is
// read, whoever called for it; the generic creator's arrow constructor is
// where an arrow that did not come through the key handler is made. See
// GameAddresses.h for the run that showed the player's spell and arrow do
// not come through the handler.
Site g_key;
Site g_factory;
Site g_arrow;

AimSourcePose g_pose;

// The rotation as it was on the way in, and whether it is currently swapped.
// g_swapped is also the reentrancy guard the stubs rely on: a second entry
// while one stands - the factory inside the handler, say - is left alone
// rather than overwriting the saved values, and only the site that swapped
// puts them back on its way out (g_swapOwner).
PlayerRotation g_saved{};
bool g_swapped = false;
const Site* g_swapOwner = nullptr;

bool g_reported = false;

// Big enough for the 46-byte stub and the two slots.
constexpr UInt32 kStubCapacity = 64;

// The player, on the same terms PlayerAim follows a pointer.
UInt32 PlayerAddressOrZero() {
	const UInt32 address = *reinterpret_cast<const UInt32*>(addr::kPlayerPointer);
	if (!mem::LooksLikeObjectAddress(address)) {
		return 0;
	}
	return address;
}

UInt32 ArgumentsOf(const Site& site) {
	return *reinterpret_cast<const UInt32*>(site.slots.arguments);
}

// Writes the return addresses found above the arguments to the log, nearest
// first - the callers of the call being wrapped. For the two probes.
void LogCallers(const char* what, UInt32 argumentsAddress) {
	const auto* stack = reinterpret_cast<const UInt32*>(argumentsAddress);
	int found = 0;
	for (UInt32 i = 0; i < 256 && found < 10; ++i) {
		const UInt32 value = stack[i];
		if (value < addr::kTextStart + 6 || value >= addr::kTextEnd) {
			continue;
		}
		const UInt8* preceding = reinterpret_cast<const UInt8*>(value - 6);
		if (!camera::LooksLikeReturnAddress(value, addr::kTextStart, addr::kTextEnd, preceding)) {
			continue;
		}
		OBVR_LOG("Aim source: %s caller %d at [args+%03X] = %08X", what, found, i * 4, value);
		++found;
	}
}

// Sets the player's rotation to the gaze and remembers what it was. Returns
// whether it did.
bool Swap(const Site& owner, const char* what) {
	if (g_swapped) {
		return false;
	}
	if (!ReadPlayerRotation(g_saved)) {
		return false;
	}

	// The same arithmetic the turn used, so the sign has the measurement
	// behind it: the head's turn is subtracted from the engine's heading.
	const float yaw = camera::PlayerYawForGaze(g_saved.yaw, g_pose.headYaw);
	if (!WritePlayerYaw(yaw)) {
		return false;
	}
	WritePlayerPitch(g_pose.pitch);
	g_swapped = true;
	g_swapOwner = &owner;

	if (!g_reported) {
		g_reported = true;
		OBVR_LOG("Aim: %s takes the gaze at the source - heading %.4f set to %.4f and pitch "
		         "%.4f to %.4f for the length of one engine call, the body never turned",
		         what, static_cast<double>(g_saved.yaw), static_cast<double>(yaw),
		         static_cast<double>(g_saved.pitch), static_cast<double>(g_pose.pitch));
	}
	return true;
}

bool BuildStub(Site& site, UInt32 target, UInt32 before, UInt32 after, const char* name,
               UInt32& stubAddress) {
	auto* stub = static_cast<UInt8*>(mem::AllocExecutable(kStubCapacity));
	if (stub == nullptr) {
		OBVR_LOG("Aim: no executable memory for the %s stub", name);
		return false;
	}
	site.slots.returnAddress = reinterpret_cast<UInt32>(stub + kStubCapacity - 8);
	site.slots.arguments = reinterpret_cast<UInt32>(stub + kStubCapacity - 4);

	stubAddress = reinterpret_cast<UInt32>(stub);
	const UInt32 size = mem::BuildAroundCallStub(
		stub, kStubCapacity - 8, stubAddress, target, before, after, site.slots);
	if (size == 0) {
		OBVR_LOG("Aim: the %s stub does not fit into %u bytes", name, kStubCapacity - 8);
		return false;
	}
	return true;
}

void InstallVtableSlot(Site& site, UInt32 slot, UInt32 target, UInt32 before, UInt32 after,
                       const char* name) {
	const UInt8 expected[4] = {static_cast<UInt8>(target & 0xFF),
	                           static_cast<UInt8>((target >> 8) & 0xFF),
	                           static_cast<UInt8>((target >> 16) & 0xFF),
	                           static_cast<UInt8>((target >> 24) & 0xFF)};
	if (!mem::Verify(slot, expected, sizeof(expected))) {
		// Another game version, or another mod there first. The slot is
		// left to whoever holds it.
		OBVR_LOG("Aim: the vtable slot at %08X does not hold %08X - %s is left as it is", slot,
		         target, name);
		return;
	}
	UInt32 stubAddress = 0;
	if (!BuildStub(site, target, before, after, name, stubAddress)) {
		return;
	}
	if (!mem::SafeWrite(slot, &stubAddress, sizeof(stubAddress))) {
		OBVR_LOG("Aim: could not write the vtable slot at %08X for %s", slot, name);
		return;
	}
	site.installed = true;
	OBVR_LOG("Aim: %s wrapped - the vtable slot at %08X goes through %08X around %08X", name,
	         slot, stubAddress, target);
}

void InstallCallSite(Site& site, UInt32 callSite, UInt32 target, UInt32 before, UInt32 after,
                     const char* name) {
	// The five bytes an untouched site holds: E8 and the displacement to the
	// function it is said to call. Anything else is another game version or
	// another mod there first, and the site is left to it.
	const UInt32 displacement = mem::CallRelativeDisplacement(callSite, target);
	const UInt8 expected[5] = {0xE8, static_cast<UInt8>(displacement & 0xFF),
	                           static_cast<UInt8>((displacement >> 8) & 0xFF),
	                           static_cast<UInt8>((displacement >> 16) & 0xFF),
	                           static_cast<UInt8>((displacement >> 24) & 0xFF)};
	if (!mem::Verify(callSite, expected, sizeof(expected))) {
		OBVR_LOG("Aim: the call at %08X is not `call %08X` - %s is left as it is", callSite,
		         target, name);
		return;
	}
	UInt32 stubAddress = 0;
	if (!BuildStub(site, target, before, after, name, stubAddress)) {
		return;
	}
	UInt8 patch[5];
	if (mem::BuildCallSitePatch(patch, sizeof(patch), callSite, stubAddress) != sizeof(patch) ||
	    !mem::SafeWrite(callSite, patch, sizeof(patch))) {
		OBVR_LOG("Aim: could not patch the call at %08X for %s", callSite, name);
		return;
	}
	site.installed = true;
	OBVR_LOG("Aim: %s wrapped - the call at %08X goes through %08X around %08X", name, callSite,
	         stubAddress, target);
}

}  // namespace

void InstallAimAtSource() {
	InstallVtableSlot(g_key, addr::kPlayerCasterVtableKeyHandlerSlot, addr::kAnimationKeyHandler,
	                  reinterpret_cast<UInt32>(&OBVR_AimSourceBeforeKey),
	                  reinterpret_cast<UInt32>(&OBVR_AimSourceAfterKey),
	                  "the player's animation-key handler");
	InstallCallSite(g_factory, addr::kCallMagicProjectileFactory, addr::kMagicProjectileFactory,
	                reinterpret_cast<UInt32>(&OBVR_AimSourceBeforeFactory),
	                reinterpret_cast<UInt32>(&OBVR_AimSourceAfterFactory),
	                "the magic projectile factory");
	InstallCallSite(g_arrow, addr::kCallArrowProjectileCreatorConstructor,
	                addr::kArrowProjectileCreatorConstructor,
	                reinterpret_cast<UInt32>(&OBVR_AimSourceBeforeArrow),
	                reinterpret_cast<UInt32>(&OBVR_AimSourceAfterArrow),
	                "the generic creator's arrow constructor probe");
}

// The key handler alone decides whether the turn machinery stands down: it
// is the site that covers the swing, and the one whose absence would leave
// nothing else covering it.
bool AimAtSourceInstalled() { return g_key.installed; }

void SetAimSourcePose(const AimSourcePose& pose) { g_pose = pose; }

}  // namespace obvr::game

// The callbacks run inside engine calls OBVR does not own: a few reads, a
// few writes, no allocation, nothing that can throw.

extern "C" void __cdecl OBVR_AimSourceBeforeKey(void* caster) {
	using namespace obvr;
	const UInt32 player = game::PlayerAddressOrZero();
	const bool isPlayer =
		player != 0 && reinterpret_cast<UInt32>(caster) == player + addr::kPlayerMagicCasterOffset;
	const SInt32 action = game::ReadPlayerAction();

	// The first entries, whatever they decide, so a run that aims nowhere
	// says whether the handler was reached at all and with what.
	static int s_entriesToLog = 8;
	if (s_entriesToLog > 0) {
		--s_entriesToLog;
		OBVR_LOG("Aim source: key handler entered - caster %08X, player %08X, action %d, "
		         "wanted %d, headYaw %.4f",
		         reinterpret_cast<UInt32>(caster), player, action, game::g_pose.wanted ? 1 : 0,
		         static_cast<double>(game::g_pose.headYaw));
	}

	if (camera::AimSourceSwapDue(game::g_pose.wanted, isPlayer, action)) {
		game::Swap(game::g_key, "a key");
	}
}

extern "C" void __cdecl OBVR_AimSourceBeforeFactory(void* caster) {
	using namespace obvr;
	const UInt32 player = game::PlayerAddressOrZero();
	const bool isPlayer =
		player != 0 && reinterpret_cast<UInt32>(caster) == player + addr::kPlayerMagicCasterOffset;

	static int s_entriesToLog = 6;
	if (s_entriesToLog > 0) {
		--s_entriesToLog;
		game::PlayerRotation now{};
		const bool read = game::ReadPlayerRotation(now);
		OBVR_LOG("Aim source: projectile factory entered - caster %08X, player %08X, wanted %d, "
		         "swapped already %d, player heading now %.4f pitch %.4f (read %d)",
		         reinterpret_cast<UInt32>(caster), player, game::g_pose.wanted ? 1 : 0,
		         game::g_swapped ? 1 : 0, static_cast<double>(now.yaw),
		         static_cast<double>(now.pitch), read ? 1 : 0);
		game::LogCallers("factory", game::ArgumentsOf(game::g_factory));
	}

	// No action gate here: the factory runs only when a projectile is made.
	if (game::g_pose.wanted && isPlayer) {
		game::Swap(game::g_factory, "a spell");
	}
}

extern "C" void __cdecl OBVR_AimSourceBeforeArrow(void* arrow) {
	using namespace obvr;
	static int s_entriesToLog = 4;
	if (s_entriesToLog > 0) {
		--s_entriesToLog;
		OBVR_LOG("Aim source: an arrow is being made through the generic creator - object %08X, "
		         "swapped already %d",
		         reinterpret_cast<UInt32>(arrow), game::g_swapped ? 1 : 0);
		game::LogCallers("arrow", game::ArgumentsOf(game::g_arrow));
	}
}

namespace obvr::game {
namespace {

// Only the site that swapped puts the rotation back: a factory entered from
// inside the key handler must not undo the handler.s swap on its way out.
void Restore(const Site& owner) {
	if (!g_swapped || g_swapOwner != &owner) {
		return;
	}
	WritePlayerYaw(g_saved.yaw);
	WritePlayerPitch(g_saved.pitch);
	g_swapped = false;
	g_swapOwner = nullptr;
}

}  // namespace
}  // namespace obvr::game

extern "C" void __cdecl OBVR_AimSourceAfterKey() { obvr::game::Restore(obvr::game::g_key); }
extern "C" void __cdecl OBVR_AimSourceAfterFactory() { obvr::game::Restore(obvr::game::g_factory); }
extern "C" void __cdecl OBVR_AimSourceAfterArrow() { obvr::game::Restore(obvr::game::g_arrow); }
