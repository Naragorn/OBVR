#include "game/Lead.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/MathFns.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "game/MeleeHits.h"

namespace obvr::game {
namespace {

constexpr UInt32 kFollowPlayerPackage = 0x0009828A;  // Oblivion.esm, "FollowPlayer"
constexpr UInt32 kLookupFormById = 0x0046B250;       // xOBSE GameAPI.cpp, cdecl, as HandBones.cpp

constexpr UInt32 kActorProcessOffset = 0x58;
constexpr UInt32 kActorExtraListOffset = 0x44;
constexpr UInt32 kProcessPackageOffset = 0x08;
constexpr UInt32 kProcessLevelSlot = 0x08;
constexpr UInt32 kProcessCurrentPackageSlot = 0x184;
constexpr UInt32 kProcessPackageEndedSlot = 0x18;
constexpr UInt32 kPackageTypeOffset = 0x20;
constexpr UInt32 kPackageTargetOffset = 0x28;
constexpr UInt32 kPackageDataOffset = 0x18;
constexpr UInt8 kPackageTypeFollow = 1;
constexpr UInt32 kActorGetPosSlot = 0x174;
constexpr UInt32 kRefRotZOffset = 0x28;
constexpr UInt32 kIsInCombatSlot = 0x334;

constexpr UInt32 kInInterruptPackage = 0x005E6B40;   // thiscall(actor) -> bool
constexpr UInt32 kPackageEvent = 0x004FBF90;         // cdecl(package, extraList, mask)
constexpr UInt32 kPackageInitData = 0x005672A0;      // thiscall(package)
constexpr UInt32 kParentCell = 0x006ECC80;           // thiscall(ref) -> cell
constexpr UInt32 kWorldspaceOf = 0x004D6670;         // thiscall(ref) -> worldspace
constexpr UInt32 kMarkPackageStart = 0x004D7A20;     // thiscall(actor, ws, cell, pos*, float rotZ), ret 10h
constexpr UInt32 kPackageSetFlag = 0x005660C0;       // thiscall(package, bool), ret 4
constexpr UInt32 kActorSetPackage = 0x005F1590;      // thiscall(actor, package, 0, 0), ret 0Ch
constexpr UInt32 kEvaluatePackage = 0x00601B80;      // thiscall(actor)
constexpr UInt32 kModDisposition = 0x005E2070;       // thiscall(actor, toward, float), ret 8

constexpr UInt8 kSetPackageBytes[] = {0x55, 0x8B, 0x6C, 0x24, 0x08, 0x56, 0x8B, 0xF1};
constexpr UInt8 kPackageEventBytes[] = {0x53, 0x56, 0x8B, 0x74, 0x24, 0x10, 0x32, 0xDB};
constexpr UInt8 kEvaluateBytes[] = {0x83, 0xEC, 0x08, 0x56, 0x8B, 0xF1, 0x83, 0x7E, 0x58, 0x00};
constexpr UInt8 kInterruptBytes[] = {0x83, 0x79, 0x58, 0x00, 0x74, 0x1A};
constexpr UInt8 kSetFlagBytes[] = {0x80, 0x7C, 0x24, 0x04, 0x00, 0x74, 0x0A};
constexpr UInt8 kMarkStartBytes[] = {0xD9, 0x44, 0x24, 0x10, 0x8B, 0x44, 0x24, 0x0C};
constexpr UInt8 kParentCellBytes[] = {0x8B, 0x41, 0x40, 0xC3};

bool LooksLikeObject(UInt32 a) { return mem::LooksLikeObjectAddress(a); }
bool LooksLikeObject(const void* p) { return LooksLikeObject(reinterpret_cast<UInt32>(p)); }
UInt32 Read(UInt32 a) { return *reinterpret_cast<const UInt32*>(a); }
UInt32 Player() { return Read(addr::kPlayerPointer); }

UInt32 Slot(UInt32 object, UInt32 offset) {
	const UInt32 vtable = LooksLikeObject(object) ? Read(object) : 0;
	if (!LooksLikeObject(vtable)) {
		return 0;
	}
	const UInt32 fn = Read(vtable + offset);
	return fn >= addr::kTextStart && fn < addr::kTextEnd ? fn : 0;
}

bool IsActor(UInt32 a) {
	if (!LooksLikeObject(a)) {
		return false;
	}
	const UInt32 vtable = Read(a);
	return vtable == addr::kVtblCharacter || vtable == addr::kVtblCreature;
}

bool NameIs(const char* name, const char* wanted) {
	if (!LooksLikeObject(reinterpret_cast<UInt32>(name) & ~3u)) {
		return false;
	}
	for (UInt32 i = 0; i < 64; ++i) {
		if (name[i] != wanted[i]) {
			return false;
		}
		if (name[i] == '\0') {
			return true;
		}
	}
	return false;
}

const UInt8* HandNode(UInt32 actor, bool right) {
	const UInt32 getNode = Slot(actor, addr::kActorVtableGetNiNodeOffset);
	if (getNode == 0) {
		return nullptr;
	}
	using NodeFn = void*(__thiscall*)(void* actor);
	void* const root = reinterpret_cast<NodeFn>(getNode)(reinterpret_cast<void*>(actor));
	const UInt32 getObject = Slot(reinterpret_cast<UInt32>(root), addr::kNiAVObjectGetObjectVtableOffset);
	if (getObject == 0) {
		return nullptr;
	}
	const char* const name = right ? "Bip01 R Hand" : "Bip01 L Hand";
	using GetObjectFn = void*(__fastcall*)(void* self, void* edx, const char* name);
	auto* const node = static_cast<const UInt8*>(reinterpret_cast<GetObjectFn>(getObject)(root, nullptr, name));
	if (!LooksLikeObject(node)) {
		return nullptr;
	}
	// NiAVObject: vtable, ..., name at +0x08.
	if (!NameIs(*reinterpret_cast<const char* const*>(node + 0x08), name)) {
		return nullptr;
	}
	return node;
}

UInt32 ProcessOf(UInt32 actor) {
	const UInt32 process = Read(actor + kActorProcessOffset);
	return LooksLikeObject(process) ? process : 0;
}

UInt32 ProcessLevel(UInt32 process) {
	const UInt32 fn = Slot(process, kProcessLevelSlot);
	if (fn == 0) {
		return 0xFFFFFFFFu;
	}
	using LevelFn = UInt32(__thiscall*)(void* process);
	return reinterpret_cast<LevelFn>(fn)(reinterpret_cast<void*>(process));
}

UInt32 CurrentPackage(UInt32 process) {
	const UInt32 fn = Slot(process, kProcessCurrentPackageSlot);
	if (fn == 0) {
		return 0;
	}
	using PackageFn = UInt32(__thiscall*)(void* process);
	const UInt32 package = reinterpret_cast<PackageFn>(fn)(reinterpret_cast<void*>(process));
	return LooksLikeObject(package) ? package : 0;
}

bool Verified() {
	static int s_verified = -1;
	if (s_verified < 0) {
		s_verified = mem::Verify(kActorSetPackage, kSetPackageBytes, sizeof(kSetPackageBytes)) &&
		                     mem::Verify(kPackageEvent, kPackageEventBytes, sizeof(kPackageEventBytes)) &&
		                     mem::Verify(kEvaluatePackage, kEvaluateBytes, sizeof(kEvaluateBytes)) &&
		                     mem::Verify(kInInterruptPackage, kInterruptBytes, sizeof(kInterruptBytes)) &&
		                     mem::Verify(kPackageSetFlag, kSetFlagBytes, sizeof(kSetFlagBytes)) &&
		                     mem::Verify(kMarkPackageStart, kMarkStartBytes, sizeof(kMarkStartBytes)) &&
		                     mem::Verify(kParentCell, kParentCellBytes, sizeof(kParentCellBytes))
		                 ? 1
		                 : 0;
		if (s_verified == 0) {
			OBVR_LOG("Lead: the package functions are not the bytes read - nobody is led");
		}
	}
	return s_verified == 1;
}

UInt32 FollowPackage() {
	using LookupFn = UInt32(__cdecl*)(UInt32 id);
	const UInt32 form = reinterpret_cast<LookupFn>(kLookupFormById)(kFollowPlayerPackage);
	return LooksLikeObject(form) ? form : 0;
}

}  // namespace

bool ActorHandPosition(void* actor, bool right, NiPoint3* out) {
	const UInt8* const node = HandNode(reinterpret_cast<UInt32>(actor), right);
	if (node == nullptr) {
		return false;
	}
	const float* const p = reinterpret_cast<const float*>(node + addr::kNodeWorldTranslateOffset);
	if (!(p[0] == p[0] && p[1] == p[1] && p[2] == p[2])) {
		return false;
	}
	*out = NiPoint3{p[0], p[1], p[2]};
	return true;
}

void* ActorHandNear(const NiPoint3& point, float searchUnits, NiPoint3* handOut, bool* rightOut) {
	NiPoint3 centre{0.0f, 0.0f, 0.0f};
	// Whoever's body the hand is at, a little wider than the shove's.
	void* const actor = LivingActorAt(point, 1.0f, searchUnits, &centre, 1.0f);
	if (actor == nullptr) {
		return nullptr;
	}
	float best = -1.0f;
	for (int side = 0; side < 2; ++side) {
		NiPoint3 hand{0.0f, 0.0f, 0.0f};
		if (!ActorHandPosition(actor, side == 0, &hand)) {
			continue;
		}
		const float d = (hand - point).LengthSquared();
		if (best < 0.0f || d < best) {
			best = d;
			*handOut = hand;
			*rightOut = side == 0;
		}
	}
	return best < 0.0f ? nullptr : actor;
}

bool ActorPosition(void* actor, NiPoint3* out) {
	const auto* const p = reinterpret_cast<const float*>(reinterpret_cast<UInt8*>(actor) + addr::kRefPositionOffset);
	*out = NiPoint3{p[0], p[1], p[2]};
	return p[0] == p[0] && p[1] == p[1] && p[2] == p[2];
}

bool ActorInCombat(void* actor) {
	const UInt32 fn = Slot(reinterpret_cast<UInt32>(actor), kIsInCombatSlot);
	if (fn == 0) {
		return false;
	}
	using InCombatFn = bool(__thiscall*)(void* actor, bool unk);
	return reinterpret_cast<InCombatFn>(fn)(actor, true);
}

bool ActorFineToLead(void* actor) {
	const UInt32 a = reinterpret_cast<UInt32>(actor);
	if (!IsActor(a)) {
		return false;
	}
	const UInt32 isDead = Slot(a, addr::kActorVtableIsDeadOffset);
	using DeadFn = bool(__thiscall*)(void* actor, UInt32 unk);
	if (isDead == 0 || reinterpret_cast<DeadFn>(isDead)(actor, 0)) {
		return false;
	}
	const UInt32 process = ProcessOf(a);
	return process != 0 && ProcessLevel(process) == 0 && !ActorInCombat(actor);
}

bool ActorIsPlayersFollower(void* actor) {
	const UInt32 process = ProcessOf(reinterpret_cast<UInt32>(actor));
	const UInt32 package = process != 0 ? CurrentPackage(process) : 0;
	if (package == 0 || *reinterpret_cast<const UInt8*>(package + kPackageTypeOffset) != kPackageTypeFollow) {
		return false;
	}
	const UInt32 target = Read(package + kPackageTargetOffset);
	if (!LooksLikeObject(target) || *reinterpret_cast<const UInt8*>(target) != 0) {
		return false;
	}
	return Read(target + 4) == Player() && Player() != 0;
}

UInt32 g_savedFollowDistance = 0;
bool g_followDistanceSet = false;

bool StartFollowing(void* actor, UInt32 followUnits) {
	const UInt32 a = reinterpret_cast<UInt32>(actor);
	const UInt32 package = FollowPackage();
	const UInt32 process = ProcessOf(a);
	if (!Verified() || package == 0 || process == 0 || ProcessLevel(process) != 0) {
		OBVR_LOG("Lead: %08X cannot be given FollowPlayer (verified %d, package %08X, process %08X)", a,
		         Verified() ? 1 : 0, package, process);
		return false;
	}
	using ActorBoolFn = bool(__thiscall*)(void* actor);
	if (reinterpret_cast<ActorBoolFn>(kInInterruptPackage)(actor)) {
		OBVR_LOG("Lead: %08X is busy (an interrupt package, talking or the like) - not led", a);
		return false;
	}
	void* const extra = reinterpret_cast<void*>(a + kActorExtraListOffset);
	using EventFn = void(__cdecl*)(UInt32 package, void* extra, UInt32 mask);
	const UInt32 current = Read(process + kProcessPackageOffset);
	if (LooksLikeObject(current)) {
		reinterpret_cast<EventFn>(kPackageEvent)(current, extra, 0x800);
	}
	reinterpret_cast<EventFn>(kPackageEvent)(package, extra, 0x200);
	if (*reinterpret_cast<const SInt32*>(package + kPackageDataOffset) == -1) {
		using PackageFn = void(__thiscall*)(void* package);
		reinterpret_cast<PackageFn>(kPackageInitData)(reinterpret_cast<void*>(package));
	}
	// The package's start marked where they stand, as the handler does.
	const UInt32 getPos = Slot(a, kActorGetPosSlot);
	if (getPos != 0) {
		using PosFn = void*(__thiscall*)(void* actor);
		using RefFn = void*(__thiscall*)(void* ref);
		using MarkFn = void(__thiscall*)(void* actor, void* ws, void* cell, void* pos, float rotZ);
		void* const pos = reinterpret_cast<PosFn>(getPos)(actor);
		void* const cell = reinterpret_cast<RefFn>(kParentCell)(actor);
		void* const ws = reinterpret_cast<RefFn>(kWorldspaceOf)(actor);
		const float rotZ = *reinterpret_cast<const float*>(a + kRefRotZOffset);
		reinterpret_cast<MarkFn>(kMarkPackageStart)(actor, ws, cell, pos, rotZ);
	}
	// How close they keep: the target's count/distance (+8 of the target
	// data; FollowPlayer's is 0, and they kept about 3 m in the first run).
	// Put back when they are let go - the package is Oblivion.esm's own.
	const UInt32 target = Read(package + kPackageTargetOffset);
	if (followUnits > 0 && LooksLikeObject(target) && !g_followDistanceSet) {
		g_savedFollowDistance = Read(target + 8);
		*reinterpret_cast<UInt32*>(target + 8) = followUnits;
		g_followDistanceSet = true;
	}
	using FlagFn = void(__thiscall*)(void* package, UInt32 on);
	reinterpret_cast<FlagFn>(kPackageSetFlag)(reinterpret_cast<void*>(package), 1);
	using SetFn = void(__thiscall*)(void* actor, void* package, UInt32 a0, UInt32 a1);
	reinterpret_cast<SetFn>(kActorSetPackage)(actor, reinterpret_cast<void*>(package), 0, 0);
	OBVR_LOG("Lead: %08X given FollowPlayer (%08X) - the package now %08X; its target %08X: %08X %08X %08X", a,
	         package, Read(process + kProcessPackageOffset), target, LooksLikeObject(target) ? Read(target) : 0,
	         LooksLikeObject(target) ? Read(target + 4) : 0, LooksLikeObject(target) ? Read(target + 8) : 0);
	return true;
}

void StopFollowing(void* actor) {
	const UInt32 a = reinterpret_cast<UInt32>(actor);
	const UInt32 package = FollowPackage();
	const UInt32 process = IsActor(a) ? ProcessOf(a) : 0;
	if (!Verified() || process == 0) {
		return;
	}
	const UInt32 current = Read(process + kProcessPackageOffset);
	if (current == package && package != 0) {
		using EventFn = void(__cdecl*)(UInt32 package, void* extra, UInt32 mask);
		reinterpret_cast<EventFn>(kPackageEvent)(current, reinterpret_cast<void*>(a + kActorExtraListOffset), 0x400);
		*reinterpret_cast<UInt32*>(process + kProcessPackageOffset) = 0;
		const UInt32 ended = Slot(process, kProcessPackageEndedSlot);
		if (ended != 0) {
			using EndedFn = void(__thiscall*)(void* process, void* actor, UInt32 flag);
			reinterpret_cast<EndedFn>(ended)(reinterpret_cast<void*>(process), actor, 1);
		}
	}
	if (g_followDistanceSet && package != 0) {
		const UInt32 target = Read(package + kPackageTargetOffset);
		if (LooksLikeObject(target)) {
			*reinterpret_cast<UInt32*>(target + 8) = g_savedFollowDistance;
		}
		g_followDistanceSet = false;
	}
	using EvalFn = void(__thiscall*)(void* actor);
	reinterpret_cast<EvalFn>(kEvaluatePackage)(actor);
	OBVR_LOG("Lead: %08X let go - FollowPlayer %s, their own AI chooses again", a,
	         current == package ? "taken away" : "was no longer theirs");
}

void ChangeDisposition(void* actor, float delta) {
	const UInt32 player = Player();
	if (!IsActor(reinterpret_cast<UInt32>(actor)) || !LooksLikeObject(player)) {
		return;
	}
	using DispositionFn = void(__thiscall*)(void* actor, void* toward, float delta);
	reinterpret_cast<DispositionFn>(kModDisposition)(actor, reinterpret_cast<void*>(player), delta);
}

}  // namespace obvr::game
