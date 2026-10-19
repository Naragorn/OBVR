#include "game/PlayerTeleport.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/ActorLink.h"
#include "game/GameAddresses.h"
#include "game/GrabPhysics.h"

namespace obvr::game {
namespace {

bool LooksLikeObject(UInt32 address) { return mem::LooksLikeObjectAddress(address); }
UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }

constexpr UInt32 kRefSetPos = 0x004D8A30;
constexpr UInt32 kControllerState = 0x0088D370;
constexpr UInt32 kControllerStateOffset = 0x1E0;
constexpr UInt32 kControllerStateSkipsPlace = 4;
constexpr UInt32 kControllerSetPosition = 0x00452A10;
constexpr UInt32 kGetNiNodeSlot = 0x154;
constexpr UInt32 kNodeLocalTranslate = 0x54;
constexpr UInt32 kNodeFlagCollision = 0x00897A20;
constexpr UInt32 kNodeUpdateWorld = 0x00707370;

constexpr UInt32 kParentCellOf = 0x006ECC80;
constexpr UInt32 kCellIsInterior = 0x004C97F0;
constexpr UInt32 kInteriorWorldOf = 0x00424180;
constexpr UInt32 kCellWorldOffset = 0x28;
constexpr UInt32 kExteriorWorldPointer = 0x00B35C24;
constexpr UInt32 kWorldPickSlot = 0x88;
constexpr UInt32 kPickSetFrom = 0x004F8840;
constexpr UInt32 kPickSetRay = 0x00663FF0;
constexpr UInt32 kPickDeriveTo = 0x0043F450;
constexpr UInt32 kPickFilterOffset = 0x24;
constexpr UInt32 kPickNormalOffset = 0x30;
constexpr UInt32 kPickFractionOffset = 0x44;
constexpr UInt32 kPickCollidableOffset = 0x50;

constexpr UInt32 kGodModeGet = 0x0065D820;
constexpr UInt32 kGodModeSet = 0x0065D810;

constexpr UInt32 kGetActorValueSlot = 0x284;   // int, current
constexpr UInt32 kGetActorValueFSlot = 0x288;  // float, current
constexpr UInt32 kGetBaseActorValue = 0x005F1910;
constexpr UInt32 kMaxEncumbrance = 0x005E0D20;
constexpr UInt32 kJumpFatigueFormula = 0x00547F60;
constexpr UInt32 kSkillMastery = 0x005F23B0;
constexpr UInt32 kSettingValue = 0x00403C00;
constexpr UInt32 kPerkJumpFatigueExpertMult = 0x00B37510;
constexpr UInt32 kModFatigue = 0x005E07D0;
constexpr UInt32 kIsInCombatSlot = 0x334;
constexpr UInt32 kMountedHorseSlot = 0x380;
constexpr UInt32 kControllerJumpApexOffset = 0x31C;

constexpr UInt32 kAvFatigue = 10;
constexpr UInt32 kAvEncumbrance = 11;
constexpr UInt32 kAvAcrobatics = 0x1A;

UInt32 Player() {
	const UInt32 player = Read(addr::kPlayerPointer);
	return LooksLikeObject(player) ? player : 0;
}

// The player once linked to its base form - the only player whose actor
// values may be read or changed (game/ActorLink.h).
UInt32 LinkedPlayer() {
	const UInt32 player = Player();
	const bool baseLooks = player != 0 && LooksLikeObject(Read(player + addr::kRefBaseFormOffset));
	return ActorValuesReadable(player != 0, baseLooks) ? player : 0;
}

UInt32 Slot(UInt32 object, UInt32 offset) {
	const UInt32 vtable = Read(object);
	if (!LooksLikeObject(vtable)) {
		return 0;
	}
	const UInt32 fn = Read(vtable + offset);
	return LooksLikeObject(fn) ? fn : 0;
}

UInt32 ControllerOf(UInt32 actor) {
	using ControllerOfFn = void*(__thiscall*)(void* actor);
	const UInt32 controller = reinterpret_cast<UInt32>(
		reinterpret_cast<ControllerOfFn>(kControllerOf)(reinterpret_cast<void*>(actor)));
	return LooksLikeObject(controller) ? controller : 0;
}

UInt32 PlayerFilter(UInt32 player, UInt32 layer) {
	using FilterOfFn = UInt32*(__thiscall*)(void* self, UInt32* out);
	UInt32 filter = 0;
	UInt32* const got =
		reinterpret_cast<FilterOfFn>(kControllerFilterOf)(reinterpret_cast<void*>(player), &filter);
	UInt32 group = got != nullptr ? FilterGroup(*got) : 0;
	if (group == 0) {
		group = kPlayerCollisionGroupFallback;
	}
	return (group << 16) | layer;
}

UInt32 PlayerWorld(UInt32 player) {
	using CellOfFn = void*(__thiscall*)(void* ref);
	using IsInteriorFn = bool(__thiscall*)(void* cell);
	using WorldOfFn = void*(__thiscall*)(void* holder);
	const UInt32 cell = reinterpret_cast<UInt32>(
		reinterpret_cast<CellOfFn>(kParentCellOf)(reinterpret_cast<void*>(player)));
	if (!LooksLikeObject(cell)) {
		return 0;
	}
	UInt32 world = 0;
	if (reinterpret_cast<IsInteriorFn>(kCellIsInterior)(reinterpret_cast<void*>(cell))) {
		world = reinterpret_cast<UInt32>(reinterpret_cast<WorldOfFn>(kInteriorWorldOf)(
			reinterpret_cast<void*>(cell + kCellWorldOffset)));
	} else {
		world = Read(kExteriorWorldPointer);
	}
	return LooksLikeObject(world) ? world : 0;
}

bool g_untouchable = false;
bool g_godModeBefore = false;

}  // namespace

UInt32 PlayerBhkWorld() {
	const UInt32 player = Read(addr::kPlayerPointer);
	return LooksLikeObject(player) ? PlayerWorld(player) : 0;
}

UInt32 PlayerCollisionGroup() {
	const UInt32 player = Read(addr::kPlayerPointer);
	return LooksLikeObject(player) ? FilterGroup(PlayerFilter(player, 0)) : kPlayerCollisionGroupFallback;
}

bool PickWorldSegment(const NiPoint3& from, const NiPoint3& to, WorldPick& out, UInt32 layer) {
	out = WorldPick{};
	const UInt32 player = Player();
	if (player == 0) {
		return false;
	}
	const UInt32 world = PlayerWorld(player);
	const UInt32 pickSlot = world != 0 ? Slot(world, kWorldPickSlot) : 0;
	if (pickSlot == 0) {
		return false;
	}
	alignas(16) UInt8 pick[0x80] = {};
	*reinterpret_cast<UInt32*>(pick + kPickFilterOffset) = PlayerFilter(player, layer);
	*reinterpret_cast<float*>(pick + kPickFractionOffset) = 1.0f;
	const float fromXyz[3] = {from.x, from.y, from.z};
	const NiPoint3 ray = to - from;
	const float rayXyz[3] = {ray.x, ray.y, ray.z};
	using SetVectorFn = void(__thiscall*)(void* pick, const float* xyz);
	using DeriveFn = void(__thiscall*)(void* pick);
	using PickFn = bool(__thiscall*)(void* world, void* pick);
	using LockFn = void(__thiscall*)(void* lock);
	reinterpret_cast<SetVectorFn>(kPickSetFrom)(pick, fromXyz);
	reinterpret_cast<SetVectorFn>(kPickSetRay)(pick, rayXyz);
	reinterpret_cast<DeriveFn>(kPickDeriveTo)(pick);
	void* const lock = reinterpret_cast<void*>(kHavokLock);
	reinterpret_cast<LockFn>(kHavokLockEnter)(lock);
	const bool asked = reinterpret_cast<PickFn>(pickSlot)(reinterpret_cast<void*>(world), pick);
	reinterpret_cast<LockFn>(kHavokLockLeave)(lock);
	const float fraction = *reinterpret_cast<const float*>(pick + kPickFractionOffset);
	const UInt32 collidable = *reinterpret_cast<const UInt32*>(pick + kPickCollidableOffset);
	if (asked && collidable != 0 && fraction >= 0.0f && fraction < 1.0f) {
		out.hit = true;
		out.fraction = fraction;
		out.collidable = collidable;
		out.point = from + ray * fraction;
		const float* const n = reinterpret_cast<const float*>(pick + kPickNormalOffset);
		out.normal = NiPoint3{n[0], n[1], n[2]};
	}
	return true;
}

bool ReadPlayerFeet(NiPoint3& out) {
	const UInt32 player = Player();
	if (player == 0) {
		return false;
	}
	out = *reinterpret_cast<const NiPoint3*>(player + addr::kRefPositionOffset);
	return out.x == out.x && out.y == out.y && out.z == out.z;
}

bool PlacePlayerAt(const NiPoint3& at) {
	const UInt32 player = Player();
	return player != 0 && PlaceActorAt(reinterpret_cast<void*>(player), at);
}

bool PlaceActorAt(void* actor, const NiPoint3& at) {
	const UInt32 player = reinterpret_cast<UInt32>(actor);
	if (!LooksLikeObject(player) || !(at.x == at.x && at.y == at.y && at.z == at.z)) {
		return false;
	}
	using SetPosFn = void(__thiscall*)(void* ref, float x, float y, float z);
	reinterpret_cast<SetPosFn>(kRefSetPos)(reinterpret_cast<void*>(player), at.x, at.y, at.z);
	const float xyz[3] = {at.x, at.y, at.z};
	const UInt32 controller = ControllerOf(player);
	if (controller != 0) {
		using StateFn = UInt32(__thiscall*)(void* state);
		using PlaceFn = void(__thiscall*)(void* controller, const float* xyz);
		const UInt32 state = reinterpret_cast<StateFn>(kControllerState)(
			reinterpret_cast<void*>(controller + kControllerStateOffset));
		if (state != kControllerStateSkipsPlace) {
			reinterpret_cast<PlaceFn>(kControllerSetPosition)(reinterpret_cast<void*>(controller), xyz);
		}
	}
	const UInt32 nodeSlot = Slot(player, kGetNiNodeSlot);
	if (nodeSlot != 0) {
		using NodeOfFn = void*(__thiscall*)(void* ref);
		const UInt32 node = reinterpret_cast<UInt32>(
			reinterpret_cast<NodeOfFn>(nodeSlot)(reinterpret_cast<void*>(player)));
		if (LooksLikeObject(node)) {
			float* const translate = reinterpret_cast<float*>(node + kNodeLocalTranslate);
			translate[0] = at.x;
			translate[1] = at.y;
			translate[2] = at.z;
			using FlagFn = void(__cdecl*)(void* node, UInt32 flag);
			using UpdateFn = void(__thiscall*)(void* node, float time, UInt32 flags);
			reinterpret_cast<FlagFn>(kNodeFlagCollision)(reinterpret_cast<void*>(node), 1);
			reinterpret_cast<UpdateFn>(kNodeUpdateWorld)(reinterpret_cast<void*>(node), 0.0f, 0);
		}
	}
	return true;
}

bool ReadPlayerFatigue(float& now, float& base) {
	const UInt32 player = LinkedPlayer();
	const UInt32 slot = player != 0 ? Slot(player, kGetActorValueFSlot) : 0;
	if (slot == 0) {
		return false;
	}
	using GetFloatFn = float(__thiscall*)(void* actor, UInt32 av);
	using GetBaseFn = SInt32(__thiscall*)(void* actor, UInt32 av);
	now = reinterpret_cast<GetFloatFn>(slot)(reinterpret_cast<void*>(player), kAvFatigue);
	base = static_cast<float>(
		reinterpret_cast<GetBaseFn>(kGetBaseActorValue)(reinterpret_cast<void*>(player), kAvFatigue));
	return now == now;
}

bool PlayerParalyzed() {
	const UInt32 player = LinkedPlayer();
	const UInt32 getInt = player != 0 ? Slot(player, kGetActorValueSlot) : 0;
	if (getInt == 0) {
		return false;
	}
	using GetIntFn = SInt32(__thiscall*)(void* actor, UInt32 av);
	return reinterpret_cast<GetIntFn>(getInt)(reinterpret_cast<void*>(player), 0x30u) != 0;
}

// The player's sex, as the script's GetIsSex reads it (its eval 0x004F6FC0,
// read 2026-10-13): the reference's base form (vtable +0x170), an NPC form
// (type 0x23), then 0x00519D20 thiscall(npc) - the flags byte at +0x28 and
// 1 (female); -1 for no NPC.
constexpr UInt32 kGetBaseFormSlot = 0x170;
constexpr UInt32 kNpcSex = 0x00519D20;
constexpr UInt8 kNpcSexBytes[] = {0x80, 0x79, 0x04, 0x23, 0x75, 0x08, 0x0F, 0xB6, 0x41, 0x28, 0x83, 0xE0, 0x01, 0xC3};
constexpr UInt8 kFormTypeNpc = 0x23;

SInt32 PlayerSex() {
	static const bool verified = mem::Verify(kNpcSex, kNpcSexBytes, sizeof(kNpcSexBytes));
	const UInt32 player = Player();
	const UInt32 getBase = player != 0 ? Slot(player, kGetBaseFormSlot) : 0;
	if (!verified || getBase == 0) {
		return -1;
	}
	using BaseFn = const UInt8*(__thiscall*)(void* ref);
	const UInt8* const form = reinterpret_cast<BaseFn>(getBase)(reinterpret_cast<void*>(player));
	if (!LooksLikeObject(reinterpret_cast<UInt32>(form)) || form[addr::kFormTypeOffset] != kFormTypeNpc) {
		return -1;
	}
	using SexFn = SInt32(__thiscall*)(const void* npc);
	return reinterpret_cast<SexFn>(kNpcSex)(form);
}

SInt32 PlayerStrength() {
	const UInt32 player = LinkedPlayer();
	const UInt32 getInt = player != 0 ? Slot(player, kGetActorValueSlot) : 0;
	if (getInt == 0) {
		return -1;
	}
	using GetIntFn = SInt32(__thiscall*)(void* actor, UInt32 av);
	return reinterpret_cast<GetIntFn>(getInt)(reinterpret_cast<void*>(player), 0u);
}

bool ReadPlayerEncumbrance(float& now) {
	const UInt32 player = LinkedPlayer();
	const UInt32 getInt = player != 0 ? Slot(player, kGetActorValueSlot) : 0;
	if (getInt == 0) {
		return false;
	}
	using GetIntFn = SInt32(__thiscall*)(void* actor, UInt32 av);
	now = static_cast<float>(reinterpret_cast<GetIntFn>(getInt)(reinterpret_cast<void*>(player), kAvEncumbrance));
	return true;
}

float PlayerDodgeFatigueCost() {
	const UInt32 player = LinkedPlayer();
	const UInt32 getInt = player != 0 ? Slot(player, kGetActorValueSlot) : 0;
	if (getInt == 0) {
		return 30.0f;  // the formula's value without load, in vanilla
	}
	using GetIntFn = SInt32(__thiscall*)(void* actor, UInt32 av);
	using MaxFn = float(__thiscall*)(void* actor);
	using FormulaFn = float(__cdecl*)(float load);
	using MasteryFn = SInt32(__thiscall*)(void* actor, UInt32 av);
	using SettingFn = const float*(__thiscall*)(void* setting);
	void* const actor = reinterpret_cast<void*>(player);
	const float encumbrance =
		static_cast<float>(reinterpret_cast<GetIntFn>(getInt)(actor, kAvEncumbrance));
	const float maxEncumbrance = reinterpret_cast<MaxFn>(kMaxEncumbrance)(actor);
	const float load = maxEncumbrance > 0.0f ? encumbrance / maxEncumbrance : 0.0f;
	float cost = reinterpret_cast<FormulaFn>(kJumpFatigueFormula)(load);
	if (reinterpret_cast<MasteryFn>(kSkillMastery)(actor, kAvAcrobatics) >= 3) {
		const float* const mult = reinterpret_cast<SettingFn>(kSettingValue)(
			reinterpret_cast<void*>(kPerkJumpFatigueExpertMult));
		if (mult != nullptr && *mult == *mult) {
			cost *= *mult;
		}
	}
	return cost == cost && cost > 0.0f ? cost : 0.0f;
}

bool SpendPlayerFatigue(float amount) {
	const UInt32 player = LinkedPlayer();
	if (player == 0 || !(amount > 0.0f)) {
		return player != 0;
	}
	using ModFn = void(__thiscall*)(void* actor, float delta);
	reinterpret_cast<ModFn>(kModFatigue)(reinterpret_cast<void*>(player), -amount);
	return true;
}

float PlayerJumpUnits() {
	const UInt32 player = Player();
	if (player == 0) {
		return 64.0f;
	}
	const UInt32 controller = ControllerOf(player);
	if (controller != 0) {
		const float apex =
			*reinterpret_cast<const float*>(controller + kControllerJumpApexOffset) / kHavokPerUnit;
		if (apex > 10.0f && apex < 2000.0f) {
			return apex;
		}
	}
	const UInt32 getInt = LinkedPlayer() != 0 ? Slot(player, kGetActorValueSlot) : 0;
	if (getInt == 0) {
		return 64.0f;
	}
	using GetIntFn = SInt32(__thiscall*)(void* actor, UInt32 av);
	return 64.0f + static_cast<float>(reinterpret_cast<GetIntFn>(getInt)(
		               reinterpret_cast<void*>(player), kAvAcrobatics));
}

bool PlayerInCombat() {
	const UInt32 player = Player();
	const UInt32 slot = player != 0 ? Slot(player, kIsInCombatSlot) : 0;
	if (slot == 0) {
		return false;
	}
	using InCombatFn = bool(__thiscall*)(void* actor, bool unk);
	return reinterpret_cast<InCombatFn>(slot)(reinterpret_cast<void*>(player), true);
}

UInt32 PlayerMountedHorse() {
	const UInt32 player = Player();
	const UInt32 slot = player != 0 ? Slot(player, kMountedHorseSlot) : 0;
	if (slot == 0) {
		return 0;
	}
	using HorseFn = void*(__thiscall*)(void* actor);
	return reinterpret_cast<UInt32>(reinterpret_cast<HorseFn>(slot)(reinterpret_cast<void*>(player)));
}

bool PlayerRiding() { return PlayerMountedHorse() != 0; }

// NorthernUI's PlayerCharacter.h: "rotation angle; probably yaw", the float
// the mounted input handler accumulates (GameAddresses.h, the heading
// virtual for the player).
constexpr UInt32 kPlayerMountedYawOffset = 0x61C;

float PlayerMountedYaw() {
	const UInt32 player = Player();
	if (player == 0) {
		return 0.0f;
	}
	return *reinterpret_cast<const float*>(player + kPlayerMountedYawOffset);
}

// A reference's rotation (rotX, rotY, rotZ in radians) at +0x20, its
// position at +0x2C (docs/vr-modding/engine-behavior.md; PlayerAim.cpp reads
// the player's the same way).
constexpr UInt32 kRefRotationOffset = 0x20;

bool ReadActorPlacement(UInt32 actor, float& rotZ, NiPoint3& position) {
	if (!mem::LooksLikeObjectAddress(actor)) {
		return false;
	}
	rotZ = *reinterpret_cast<const float*>(actor + kRefRotationOffset + 8);
	position = *reinterpret_cast<const NiPoint3*>(actor + addr::kRefPositionOffset);
	return true;
}

constexpr UInt32 kPlayerSitMountBytesOffset = 0x71C;
constexpr UInt32 kThirdPersonZoomAddress = 0x00B3BB24;

UInt16 PlayerSitMountBytes() {
	const UInt32 player = Player();
	if (player == 0) {
		return 0;
	}
	const UInt8* const bytes = reinterpret_cast<const UInt8*>(player + kPlayerSitMountBytesOffset);
	return static_cast<UInt16>(bytes[0] | (static_cast<UInt16>(bytes[1]) << 8));
}

float ThirdPersonZoom() { return *reinterpret_cast<const float*>(kThirdPersonZoomAddress); }

void SetPlayerUntouchable(bool on) {
	using GetFn = bool(__cdecl*)();
	using SetFn = void(__cdecl*)(bool on);
	if (on == g_untouchable) {
		return;
	}
	if (on) {
		g_godModeBefore = reinterpret_cast<GetFn>(kGodModeGet)();
		reinterpret_cast<SetFn>(kGodModeSet)(true);
	} else {
		reinterpret_cast<SetFn>(kGodModeSet)(g_godModeBefore);
	}
	g_untouchable = on;
}

}  // namespace obvr::game
