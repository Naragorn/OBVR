#include "game/BladeBodies.h"

#include "core/AddressSpace.h"
#include "core/Config.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/FittedCapsules.h"
#include "game/GameAddresses.h"
#include "game/GameTypes.h"
#include "game/HandContactLogic.h"
#include "game/PlayerTeleport.h"
#include "game/ShieldShape.h"
#include "platform/Win32Min.h"

namespace obvr::game {
namespace {

UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }
bool LooksLikeObject(UInt32 address) { return mem::LooksLikeObjectAddress(address); }

UInt32 Slot(UInt32 object, UInt32 offset) {
	const UInt32 vtable = LooksLikeObject(object) ? Read(object) : 0;
	if (!LooksLikeObject(vtable)) {
		return 0;
	}
	const UInt32 fn = Read(vtable + offset);
	return fn >= addr::kTextStart && fn < addr::kTextEnd ? fn : 0;
}

struct ListNode {
	void* data;
	ListNode* next;
};

using ListFn = void*(__fastcall*)(void* self, void* edx, UInt32 level);
using NodeFn = void*(__fastcall*)(void* self, void* edx);
using DeadFn = UInt8(__fastcall*)(void* self, void* edx, UInt32 arg);
using GetObjectFn = void*(__fastcall*)(void* self, void* edx, const char* name);

bool IsLivingActor(UInt32 actor, UInt32 player, UInt32 mount) {
	if (!ActorMayBeABody(actor, player, mount) || !LooksLikeObject(actor)) {
		return false;
	}
	const UInt32 vtable = Read(actor);
	if (vtable != addr::kVtblCharacter && vtable != addr::kVtblCreature) {
		return false;
	}
	const UInt32 dead = Slot(actor, addr::kActorVtableIsDeadOffset);
	return dead != 0 && (reinterpret_cast<DeadFn>(dead)(reinterpret_cast<void*>(actor), nullptr, 0) & 1) == 0;
}

bool IsBone(const NiAVObject* node) {
	if (!LooksLikeObject(reinterpret_cast<UInt32>(node))) {
		return false;
	}
	const char* const name = node->name;
	if (!LooksLikeObject(reinterpret_cast<UInt32>(name) & ~3u)) {
		return false;
	}
	return name[0] == 'B' && name[1] == 'i' && name[2] == 'p' && name[3] == '0' && name[4] == '1';
}

// The bones kept per actor and root: a new root (a load, a new 3D) is read
// anew.
struct Kept {
	UInt32 actor = 0;
	UInt32 root = 0;
	const NiAVObject* bone[kBladeBoneCount] = {};
	// Their drawn weapon's node, and their blade the frame before.
	const NiAVObject* weapon = nullptr;
	bool haveBlade = false;
	NiPoint3 bladeA{0.0f, 0.0f, 0.0f};
	NiPoint3 bladeB{0.0f, 0.0f, 0.0f};
	UInt32 lastFrame = 0;
	// The twist bone a shield hangs on, and the shield's disc read from the
	// model under it (game/ShieldShape, in the bone's own frame) - read again
	// when the bone's bound changes (a shield put on or taken off) or every
	// so often (HandContactLogic.h, ShieldDiscStale).
	const NiAVObject* twist = nullptr;
	bool discTried = false;
	bool discHave = false;
	UInt32 discFrame = 0;
	float discBound = 0.0f;
	ShieldDisc disc;
	// The capsules fitted to what they wear ([Hands] FittedBodyCapsules,
	// game/FittedCapsules.h), the print of the shapes they were fitted to,
	// and the collect when the print was last taken.
	FittedBody fitted;
	UInt32 fitPrint = 0;
	UInt32 printCall = 0;
	bool printTaken = false;
};

constexpr const char* kTwistBoneName = "Bip01 L ForearmTwist";

bool IsWeaponNode(const NiAVObject* node) {
	if (!LooksLikeObject(reinterpret_cast<UInt32>(node))) {
		return false;
	}
	const char* const name = node->name;
	if (!LooksLikeObject(reinterpret_cast<UInt32>(name) & ~3u)) {
		return false;
	}
	return name[0] == 'W' && name[1] == 'e' && name[2] == 'a' && name[3] == 'p' && name[4] == 'o' && name[5] == 'n' &&
	       name[6] == '\0';
}

// The actor's process action (HighProcess +0x1F4, GameAddresses.h), only in
// high process (level 0, the process's vtable +0x08, as leading reads it):
// a lower process has no such field.
SInt32 ActionOf(UInt32 actor) {
	const UInt32 process = Read(actor + addr::kMobileProcessOffset);
	if (!LooksLikeObject(process)) {
		return addr::kActionNone;
	}
	const UInt32 levelFn = Slot(process, 0x08);
	if (levelFn == 0) {
		return addr::kActionNone;
	}
	using LevelFn = UInt32(__fastcall*)(void* process, void* edx);
	if (reinterpret_cast<LevelFn>(levelFn)(reinterpret_cast<void*>(process), nullptr) != 0) {
		return addr::kActionNone;
	}
	return *reinterpret_cast<const SInt16*>(process + addr::kProcessCurrentActionOffset);
}

constexpr UInt32 kKeptMax = 16;
Kept g_kept[kKeptMax];
UInt32 g_frame = 0;
UInt32 g_readLines = 8;

// The fitted capsules: what they wear is printed again every this many
// collects (three a frame: the grapple's, the blade's and the hands'; at 75
// frames a second about one second), and a fit - near a millisecond for a
// body of 5000-8000 vertices (spec 4.7, the C++ fit timed offline) - runs
// at most once in this many milliseconds, so people coming into view are
// fitted one after the other, not all in one frame.
constexpr UInt32 kPrintEveryCollects = 225;
constexpr double kFitSpacingMilliseconds = 8.0;
long long g_lastFit = 0;
UInt32 g_fitLines = 8;

bool FitSpacingAllows() {
	const long long now = ReadPerformanceCounter();
	const long long frequency = ReadPerformanceFrequency();
	if (frequency > 0 && g_lastFit != 0 &&
	    static_cast<double>(now - g_lastFit) * 1000.0 / static_cast<double>(frequency) < kFitSpacingMilliseconds) {
		return false;
	}
	g_lastFit = now;
	return true;
}

// Refits an actor's capsules when what they wear changed (or was never
// fitted), as the spacing allows; a fit that finds nothing leaves the
// guessed capsules in play until the print changes again.
void StepFitted(Kept& k, UInt32 actor, const NiAVObject* root) {
	if (k.printTaken && g_frame - k.printCall < kPrintEveryCollects) {
		return;
	}
	const UInt32 print = ActorOutfitPrint(root);
	if (k.printTaken && print == k.fitPrint) {
		k.printCall = g_frame;
		return;
	}
	if (!FitSpacingAllows()) {
		return;  // asked again at the next collect
	}
	k.printTaken = true;
	k.printCall = g_frame;
	k.fitPrint = print;
	char why[200] = "";
	const bool fitted = FitActorBody(root, k.fitted, why, sizeof(why));
	const bool log = GetConfig().hands.capsuleTrace || g_fitLines > 0;
	if (!log) {
		return;
	}
	if (g_fitLines > 0) {
		--g_fitLines;
	}
	if (!fitted) {
		OBVR_LOG("Capsules: %08X NOT fitted - %s; the guessed capsules stay (%.2f ms)", actor, why,
		         k.fitted.milliseconds);
		return;
	}
	char radii[640];
	DescribeFittedBody(k.fitted, radii, sizeof(radii));
	OBVR_LOG("Capsules: %08X fitted - %u shapes, %u vertices, %u capsules in %.2f ms: %s%s%s", actor, k.fitted.shapes,
	         k.fitted.vertices, k.fitted.count, k.fitted.milliseconds, radii, why[0] != '\0' ? "; " : "", why);
	if (!GetConfig().hands.capsuleTrace) {
		return;
	}
	// where each lands now, from the model's root: what the harness checks
	// against the body (a torso at chest height, the feet at the floor)
	const NiPoint3 at = root->worldTransform.pos;
	for (UInt32 i = 0; i < k.fitted.count; ++i) {
		if (!IsBone(k.fitted.frame[i])) {
			continue;
		}
		NiPoint3 a;
		NiPoint3 b;
		float radius = 0.0f;
		PlaceFittedCapsule(k.fitted.frame[i]->worldTransform, k.fitted.cap[i], a, b, radius);
		a = a - at;
		b = b - at;
		OBVR_LOG("Capsules: %08X %s from %.1f %.1f %.1f to %.1f %.1f %.1f, %.1f round (from the root, %u vertices)", actor,
		         FitGroupInfoOf(k.fitted.cap[i].group).name, static_cast<double>(a.x), static_cast<double>(a.y),
		         static_cast<double>(a.z), static_cast<double>(b.x), static_cast<double>(b.y), static_cast<double>(b.z),
		         static_cast<double>(radius), k.fitted.cap[i].vertices);
	}
}

// The fitted capsules placed by their frame bones; false when one of the
// bones no longer reads (the guessed ones are taken this frame instead).
bool PlaceFitted(const Kept& k, UInt32 actor, BladeBodies& out) {
	if (k.fitted.count == 0) {
		return false;
	}
	for (UInt32 i = 0; i < k.fitted.count; ++i) {
		if (!IsBone(k.fitted.frame[i])) {
			return false;
		}
	}
	for (UInt32 i = 0; i < k.fitted.count; ++i) {
		NiPoint3 a;
		NiPoint3 b;
		float radius = 0.0f;
		PlaceFittedCapsule(k.fitted.frame[i]->worldTransform, k.fitted.cap[i], a, b, radius);
		out.Add(a, b, radius, actor, k.fitted.cap[i].part);
	}
	return true;
}

Kept& KeptFor(UInt32 actor, UInt32 root) {
	Kept* oldest = &g_kept[0];
	for (Kept& k : g_kept) {
		if (k.actor == actor && k.root == root) {
			k.lastFrame = g_frame;
			return k;
		}
		if (k.actor == actor || k.lastFrame < oldest->lastFrame) {
			oldest = &k;
			if (k.actor == actor) {
				break;
			}
		}
	}
	Kept& k = *oldest;
	k = Kept{};
	k.actor = actor;
	k.root = root;
	k.lastFrame = g_frame;
	const UInt32 getObject = Slot(root, addr::kNiAVObjectGetObjectVtableOffset);
	UInt32 found = 0;
	for (UInt32 b = 0; b < kBladeBoneCount && getObject != 0; ++b) {
		auto* const node = static_cast<const NiAVObject*>(
			reinterpret_cast<GetObjectFn>(getObject)(reinterpret_cast<void*>(root), nullptr, BladeBoneName(b)));
		if (IsBone(node)) {
			k.bone[b] = node;
			++found;
		}
	}
	if (getObject != 0) {
		auto* const weapon = static_cast<const NiAVObject*>(
			reinterpret_cast<GetObjectFn>(getObject)(reinterpret_cast<void*>(root), nullptr, "Weapon"));
		k.weapon = IsWeaponNode(weapon) ? weapon : nullptr;
		auto* const twist = static_cast<const NiAVObject*>(
			reinterpret_cast<GetObjectFn>(getObject)(reinterpret_cast<void*>(root), nullptr, kTwistBoneName));
		k.twist = IsBone(twist) ? twist : nullptr;
	}
	if (g_readLines > 0) {
		--g_readLines;
		char missing[160] = {};
		UInt32 at = 0;
		for (UInt32 b = 0; b < kBladeBoneCount; ++b) {
			if (k.bone[b] != nullptr) {
				continue;
			}
			const char* name = BladeBoneName(b) + 6;  // past "Bip01 "
			for (; *name != '\0' && at + 2 < sizeof(missing); ++name) {
				missing[at++] = *name;
			}
			if (at + 2 < sizeof(missing)) {
				missing[at++] = ',';
			}
		}
		missing[at > 0 ? at - 1 : 0] = '\0';
		OBVR_LOG("Contact: %08X's skeleton read - %u of %u bones%s%s%s; a weapon node %s", actor, found,
		         static_cast<UInt32>(kBladeBoneCount), found < kBladeBoneCount ? " (none for " : "", missing,
		         found < kBladeBoneCount ? ")" : "", k.weapon != nullptr ? "found" : "NOT found");
	}
	return k;
}

}  // namespace

UInt32 CollectBladeBodies(const NiPoint3& around, float withinUnits, BladeBodies& out, BladeFoes* foes) {
	++g_frame;
	out.count = 0;
	if (foes != nullptr) {
		foes->count = 0;
	}
	const UInt32 player = Read(addr::kPlayerPointer);
	if (!LooksLikeObject(player)) {
		return 0;
	}
	const UInt32 mount = PlayerMountedHorse();
	auto* const manager = reinterpret_cast<void*>(addr::kActorProcessManager);
	auto* node = static_cast<ListNode*>(reinterpret_cast<ListFn>(addr::kActorListByLevel)(manager, nullptr, 0));
	UInt32 people = 0;
	for (UInt32 visited = 0; node != nullptr && visited < 512; ++visited) {
		if (!LooksLikeObject(reinterpret_cast<UInt32>(node))) {
			break;
		}
		const UInt32 actor = reinterpret_cast<UInt32>(node->data);
		node = node->next;
		if (!IsLivingActor(actor, player, mount)) {
			continue;
		}
		const UInt32 getNode = Slot(actor, addr::kActorVtableGetNiNodeOffset);
		if (getNode == 0) {
			continue;
		}
		const UInt32 root =
			reinterpret_cast<UInt32>(reinterpret_cast<NodeFn>(getNode)(reinterpret_cast<void*>(actor), nullptr));
		if (!LooksLikeObject(root)) {
			continue;
		}
		const auto* const rootNode = reinterpret_cast<const NiAVObject*>(root);
		const NiBound bound = rootNode->worldBound;
		if (!(bound.radius > 0.0f && bound.radius < 4096.0f)) {
			continue;
		}
		const NiPoint3 off = bound.center - around;
		const float reach = withinUnits + bound.radius;
		if (!(off.LengthSquared() <= reach * reach)) {
			continue;
		}
		Kept& k = KeptFor(actor, root);
		// Their blade, when a weapon is in their hand, and what they are doing.
		NiPoint3 bladeA{0.0f, 0.0f, 0.0f};
		NiPoint3 bladeB{0.0f, 0.0f, 0.0f};
		const bool blade =
			k.weapon != nullptr && IsWeaponNode(k.weapon) &&
			TheirBladeFromNode(k.weapon->worldTransform.pos,
			                   NiPoint3{k.weapon->worldTransform.rot.data[0][1], k.weapon->worldTransform.rot.data[1][1],
			                            k.weapon->worldTransform.rot.data[2][1]},
			                   k.weapon->worldBound.center, k.weapon->worldBound.radius, bladeA, bladeB);
		// Their shield, when one hangs on the twist bone: its disc, in the
		// world. A bare bone has nothing under it to read; a bound that
		// changes is a shield put on or taken off.
		bool shield = false;
		BladeDisc shieldWorld;
		if (foes != nullptr && k.twist != nullptr && IsBone(k.twist)) {
			const float twistBound = k.twist->worldBound.radius;
			const UInt32 twistAddress = reinterpret_cast<UInt32>(k.twist);
			if (ShieldDiscStale(k.discTried, twistAddress, twistAddress, k.discBound, twistBound, g_frame - k.discFrame)) {
				const char* why = "";
				const bool had = k.discHave;
				k.discHave = ReadShieldDisc(k.twist, k.twist->worldTransform.rot, k.twist->worldTransform.pos, k.disc, &why);
				k.discTried = true;
				k.discFrame = g_frame;
				k.discBound = twistBound;
				static UInt32 s_shieldLines = 8;
				if (k.discHave && !had && s_shieldLines > 0) {
					--s_shieldLines;
					OBVR_LOG("Contact: %08X's shield read - %u vertices, spokes %.1f and %.1f units, %.1f thick (the bound "
					         "%.1f round)",
					         actor, k.disc.vertices, static_cast<double>(k.disc.half[0]), static_cast<double>(k.disc.half[1]),
					         static_cast<double>(k.disc.half[2] * 2.0f), static_cast<double>(twistBound));
				}
			}
			if (k.discHave) {
				shieldWorld = ShieldDiscInWorld(k.disc, k.twist->worldTransform.rot, k.twist->worldTransform.pos,
				                                FoeShieldId(actor));
				shield = true;
			}
		}
		if ((blade || shield) && foes != nullptr && foes->count < kBladeFoesMax) {
			BladeFoe& f = foes->foe[foes->count++];
			f.actor = actor;
			f.action = ActionOf(actor);
			f.bladeValid = blade;
			f.a = bladeA;
			f.b = bladeB;
			f.haveLast = blade && k.haveBlade;
			f.lastA = blade && k.haveBlade ? k.bladeA : bladeA;
			f.lastB = blade && k.haveBlade ? k.bladeB : bladeB;
			f.shieldValid = shield;
			f.shieldCentre = shieldWorld.centre;
			f.shieldNormal = shieldWorld.normal;
			f.shieldRadius = shieldWorld.radius;
			f.shieldHalfThick = shieldWorld.halfThick;
		}
		k.haveBlade = blade;
		k.bladeA = bladeA;
		k.bladeB = bladeB;
		if (GetConfig().hands.fittedBodyCapsules) {
			StepFitted(k, actor, rootNode);
			if (PlaceFitted(k, actor, out)) {
				++people;
				continue;
			}
		}
		NiPoint3 at[kBladeBoneCount];
		bool have[kBladeBoneCount];
		for (UInt32 b = 0; b < kBladeBoneCount; ++b) {
			have[b] = k.bone[b] != nullptr && IsBone(k.bone[b]);
			at[b] = have[b] ? k.bone[b]->worldTransform.pos : NiPoint3{0.0f, 0.0f, 0.0f};
		}
		if (BodyCapsulesFromBones(at, have, rootNode->worldTransform.scale, actor, out) == 0 &&
		    !BodyColumnFromBound(bound.center, bound.radius, actor, out)) {
			continue;
		}
		++people;
	}
	return people;
}

void ForgetBladeBodies() {
	for (Kept& k : g_kept) {
		k = Kept{};
	}
}

bool ReadBlockBlow(UInt32 attacker, BlockBlow& out) {
	out = BlockBlow{};
	const UInt32 player = Read(addr::kPlayerPointer);
	if (!IsLivingActor(attacker, player, PlayerMountedHorse())) {
		return false;
	}
	const UInt32 getNode = Slot(attacker, addr::kActorVtableGetNiNodeOffset);
	if (getNode == 0) {
		return false;
	}
	const UInt32 root =
		reinterpret_cast<UInt32>(reinterpret_cast<NodeFn>(getNode)(reinterpret_cast<void*>(attacker), nullptr));
	if (!LooksLikeObject(root)) {
		return false;
	}
	const auto* const rootNode = reinterpret_cast<const NiAVObject*>(root);
	const NiBound bound = rootNode->worldBound;
	if (bound.radius > 0.0f && bound.radius < 4096.0f) {
		out.centreValid = true;
		out.centre = bound.center;
		out.centreRadius = bound.radius;
	}
	const Kept& k = KeptFor(attacker, root);
	out.bladeValid =
		k.weapon != nullptr && IsWeaponNode(k.weapon) &&
		TheirBladeFromNode(k.weapon->worldTransform.pos,
		                   NiPoint3{k.weapon->worldTransform.rot.data[0][1], k.weapon->worldTransform.rot.data[1][1],
		                            k.weapon->worldTransform.rot.data[2][1]},
		                   k.weapon->worldBound.center, k.weapon->worldBound.radius, out.bladeA, out.bladeB);
	return true;
}

const NiAVObject* ActorBoneNode(UInt32 actor, UInt32 root, UInt32 bone) {
	if (actor == 0 || !LooksLikeObject(root) || bone >= kBladeBoneCount) {
		return nullptr;
	}
	const Kept& k = KeptFor(actor, root);
	return k.bone[bone] != nullptr && IsBone(k.bone[bone]) ? k.bone[bone] : nullptr;
}

const NiAVObject* BladeBoneNode(UInt32 actor, UInt32 bone) {
	if (actor == 0 || bone >= kBladeBoneCount) {
		return nullptr;
	}
	for (const Kept& k : g_kept) {
		if (k.actor == actor) {
			return k.bone[bone] != nullptr && IsBone(k.bone[bone]) ? k.bone[bone] : nullptr;
		}
	}
	return nullptr;
}

}  // namespace obvr::game
