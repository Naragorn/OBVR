#include "game/GearWear.h"
#include "core/AddressSpace.h"
#include "core/AroundCall.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/ConsoleLine.h"
#include "game/GameAddresses.h"
#include "game/GrabPhysics.h"
#include "game/MeleeHits.h"
#include "game/RefFingerprint.h"


namespace obvr::game {

namespace {

// Read 2026-11-11 in Oblivion.exe 1.2.0.416 (game/GearWearLogic.h).
constexpr UInt32 kWeaponWearSetting = 0x00B36E10;  // fDamageToWeaponPercentage's value
constexpr UInt32 kArmorWearSetting = 0x00B36E18;   // fDamageToArmorPercentage's value
constexpr UInt32 kPickArmorCall = 0x005FFC1B;      // call 0x005E5A00, the random worn piece
constexpr UInt32 kPickArmor = 0x005E5A00;
constexpr UInt32 kDropRecordCall = 0x005F3AF9;     // call 0x004DC000 (actor, ref): the drop's bookkeeping
constexpr UInt32 kDropRecord = 0x004DC000;
constexpr UInt32 kDamageItem = 0x005F3870;         // Actor's item damage, vtable +0x2C4
constexpr UInt32 kPlayerDamageItem = 0x0065FF10;   // the player's, calling it
constexpr UInt32 kSlotCharacter = 0x00A6FC9C + 0x2C4;
constexpr UInt32 kSlotCreature = 0x00A710F4 + 0x2C4;
constexpr UInt32 kSlotPlayer = 0x00A73A0C + 0x2C4;
constexpr UInt32 kContainerChanges = 0x0041E6F0;   // ExtraDataList -> ExtraContainerChanges data
constexpr UInt32 kWornInSlot = 0x00486790;         // (slot, armorOnly) -> a new entry, as the picker does
constexpr UInt32 kEntryHealth = 0x00484850;        // entry's health
constexpr UInt32 kActorDropOffset = 0x2C8;         // Actor vtable: drop (form, extra, count, pos*, rot*) -> ref
constexpr UInt32 kFormIsQuestItemOffset = 0x78;    // TESForm vtable, as the engine's own drop asks
constexpr UInt32 kRefExtraListOffset = 0x44;

constexpr UInt8 kDamageItemBytes[] = {0x83, 0xEC, 0x18, 0x56, 0x57, 0x8B, 0x7C, 0x24, 0x24, 0x8B, 0xF1, 0x32};
constexpr UInt8 kWornInSlotBytes[] = {0x83, 0xEC, 0x0C, 0x8B, 0x09, 0x53, 0x55, 0x33, 0xED, 0x3B, 0xCD, 0x56};
constexpr UInt8 kContainerChangesBytes[] = {0x6A, 0x1A, 0xE8, 0x19, 0xFB, 0xFF, 0xFF, 0x85, 0xC0, 0x74, 0x04, 0x8B};
constexpr UInt8 kEntryHealthBytes[] = {0x83, 0xEC, 0x08, 0x56, 0x57, 0x6A, 0x00, 0x68, 0xF4, 0x5C, 0xB0, 0x00};

using PickArmorFn = void*(__fastcall*)(void* actor, void* edx);
using DropRecordFn = void(__cdecl*)(void* actor, void* ref);
using DamageItemFn = bool(__fastcall*)(void* actor, void* edx, UInt32* entry, float amount, UInt32 skip);
using ChangesFn = void*(__fastcall*)(void* extraList, void* edx);
using WornInSlotFn = UInt32*(__fastcall*)(void* changes, void* edx, SInt32 slot, UInt32 armorOnly);
using HealthFn = float(__fastcall*)(UInt32* entry, void* edx, UInt32 flag);
using DropFn = void*(__fastcall*)(void* actor, void* edx, void* form, void* extra, UInt32 count, NiPoint3* pos,
                                   NiPoint3* rot);
using BoolThisFn = bool(__fastcall*)(void* self, void* edx);

struct Strike {
	void* target = nullptr;
	bool parted = false;
	BodyPart part = BodyPart::Column;
	NiPoint3 way{0.0f, 0.0f, 0.0f};
};

constexpr UInt32 kKnockedMax = 16;
struct Knocked {
	UInt32 ref = 0;
	RefFingerprint print;  // a load or a cell gone takes the reference with it
	NiPoint3 velocity{0.0f, 0.0f, 0.0f};
	KnockedPiece step;
};

struct State {
	bool pickRouted = false;
	bool damageRouted = false;
	bool dropWatched = false;
	bool helpersAsRead = false;
	Strike strike;
	GearWearSettings settings;
	GmstScale armorScale;
	GmstScale weaponScale;
	// Inside an actor's item damage: the engine's own drop is taken, not
	// recorded as the actor's dropped item, and its ref goes here.
	UInt32 damageDepth = 0;
	UInt32 engineDroppedRef = 0;
	Knocked knocked[kKnockedMax];
	UInt32 lines = 60;
};

State& S() {
	static State s;
	return s;
}

void* PlayerActor() {
	const UInt32 p = *reinterpret_cast<const UInt32*>(addr::kPlayerPointer);
	return mem::LooksLikeObjectAddress(p) ? reinterpret_cast<void*>(p) : nullptr;
}

void* ChangesOf(void* actor) {
	return reinterpret_cast<ChangesFn>(kContainerChanges)(static_cast<UInt8*>(actor) + kRefExtraListOffset, nullptr);
}

const char* PartSlotName(SInt32 slot) {
	switch (slot) {
	case kSlotHead: return "helmet";
	case kSlotUpperBody: return "cuirass";
	case kSlotLowerBody: return "greaves";
	case kSlotHand: return "gauntlets";
	case kSlotFoot: return "boots";
	default: return "none";
	}
}

void* __fastcall OnPickArmor(void* actor, void* edx) {
	State& s = S();
	if (!s.strike.parted || actor != s.strike.target || !s.helpersAsRead) {
		return reinterpret_cast<PickArmorFn>(kPickArmor)(actor, edx);
	}
	void* const changes = ChangesOf(actor);
	const WearSlots slots = ArmorSlotsForPart(s.strike.part);
	UInt32* entry = nullptr;
	SInt32 slot = slots.first;
	if (changes != nullptr) {
		entry = reinterpret_cast<WornInSlotFn>(kWornInSlot)(changes, nullptr, slots.first, 1);
		if (entry == nullptr && slots.second != kSlotNone) {
			slot = slots.second;
			entry = reinterpret_cast<WornInSlotFn>(kWornInSlot)(changes, nullptr, slots.second, 1);
		}
	}
	if (s.lines > 0) {
		--s.lines;
		OBVR_LOG("Wear: the strike on %08X's %s wears its %s%s", reinterpret_cast<UInt32>(actor),
		         BodyPartName(s.strike.part), PartSlotName(slot), entry != nullptr ? "" : " - none worn there, no wear");
	}
	return entry;
}

void __cdecl OnDropRecord(void* actor, void* ref) {
	State& s = S();
	if (s.damageDepth > 0) {
		// Not the actor's dropped item: it flies and is gone (OnDamageItem).
		s.engineDroppedRef = reinterpret_cast<UInt32>(ref);
		return;
	}
	reinterpret_cast<DropRecordFn>(kDropRecord)(actor, ref);
}

void Knock(UInt32 ref, const NiPoint3& way) {
	State& s = S();
	for (Knocked& k : s.knocked) {
		if (k.ref == 0) {
			k = Knocked{};
			k.ref = ref;
			k.print = TakeRefFingerprint(ref);
			k.velocity = KnockOffVelocity(way, 260.0f);
			return;
		}
	}
	// No room: gone at once rather than left as an item.
	RequestConsoleLineAs(ref, "disable");
}

UInt32 DropForKnock(void* actor, UInt32* entry) {
	void* const form = reinterpret_cast<void*>(entry[2]);
	const UInt32 list = entry[0];
	void* const extra = mem::LooksLikeObjectAddress(list) ? *reinterpret_cast<void**>(list) : nullptr;
	const float* p = reinterpret_cast<const float*>(static_cast<UInt8*>(actor) + addr::kRefPositionOffset);
	NiPoint3 at{p[0], p[1], p[2] + 100.0f};
	NiPoint3 rot{0.0f, 0.0f, 0.0f};
	const UInt32 drop = (*reinterpret_cast<UInt32* const*>(actor))[kActorDropOffset / 4];
	return reinterpret_cast<UInt32>(reinterpret_cast<DropFn>(drop)(actor, nullptr, form, extra, 1, &at, &rot));
}

bool __fastcall OnDamageItem(void* actor, void* edx, UInt32* entry, float amount, UInt32 skip) {
	State& s = S();
	const bool isPlayer = actor == PlayerActor();
	void* const form = mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(entry)) ? reinterpret_cast<void*>(entry[2])
	                                                                                    : nullptr;
	++s.damageDepth;
	s.engineDroppedRef = 0;
	const bool result = reinterpret_cast<DamageItemFn>(isPlayer ? kPlayerDamageItem : kDamageItem)(actor, edx, entry,
	                                                                                               amount, skip);
	--s.damageDepth;
	const UInt32 dropped = s.engineDroppedRef;
	s.engineDroppedRef = 0;
	if (form == nullptr || !s.helpersAsRead) {
		return result;
	}
	const float health = dropped != 0 ? 0.0f : reinterpret_cast<HealthFn>(kEntryHealth)(entry, nullptr, 0);
	if (s.lines > 0) {
		--s.lines;
		const bool armor = *reinterpret_cast<const UInt8*>(static_cast<UInt8*>(form) + addr::kFormTypeOffset) == 0x14;
		OBVR_LOG("Wear: %08X's %08X takes %.2f (%s wear x%.1f), now %.1f", reinterpret_cast<UInt32>(actor),
		         *reinterpret_cast<const UInt32*>(static_cast<UInt8*>(form) + addr::kFormIdOffset),
		         static_cast<double>(amount), armor ? "armor" : "weapon",
		         static_cast<double>(ClampWearMult(armor ? s.settings.armorWearMult : s.settings.weaponWearMult)),
		         static_cast<double>(health));
	}
	if (health > 0.0f) {
		return result;
	}
	const UInt32 formVtbl = *reinterpret_cast<const UInt32*>(form);
	const bool quest = reinterpret_cast<BoolThisFn>(*reinterpret_cast<const UInt32*>(formVtbl + kFormIsQuestItemOffset))(
		form, nullptr);
	const GearBreakMode mode = GearBreakModeOf(s.settings.playerGearBreaksOff);
	const ZeroAction act = quest && dropped == 0 ? ZeroAction::Leave
	                                             : ZeroHealthAction(isPlayer, mode, dropped == 0, dropped == 0, dropped != 0);
	const NiPoint3 way = actor == s.strike.target ? s.strike.way : NiPoint3{0.0f, 0.0f, 0.0f};
	UInt32 ref = dropped;
	switch (act) {
	case ZeroAction::KnockDropped: Knock(dropped, way); break;
	case ZeroAction::DropAndKnock:
		ref = DropForKnock(actor, entry);
		if (mem::LooksLikeObjectAddress(ref)) {
			Knock(ref, way);
		}
		break;
	case ZeroAction::ToInventory: UnequipItemForm(static_cast<UInt8*>(form), false); break;
	case ZeroAction::Leave:
	default: break;
	}
	if (s.lines > 0) {
		--s.lines;
		static const char* const kAct[] = {"left", "knocked off (the engine's drop)", "knocked off", "into the inventory"};
		OBVR_LOG("Wear: %08X's %08X at 0 - %s%s (ref %08X)", reinterpret_cast<UInt32>(actor),
		         *reinterpret_cast<const UInt32*>(static_cast<UInt8*>(form) + addr::kFormIdOffset),
		         kAct[static_cast<UInt32>(act)], quest ? ", a quest item" : "", ref);
	}
	return result;
}

bool RouteCall(UInt32 site, UInt32 target, UInt32 stub, const char* what) {
	const UInt32 d = mem::CallRelativeDisplacement(site, target);
	const UInt8 expected[5] = {0xE8, static_cast<UInt8>(d & 0xFF), static_cast<UInt8>((d >> 8) & 0xFF),
	                           static_cast<UInt8>((d >> 16) & 0xFF), static_cast<UInt8>((d >> 24) & 0xFF)};
	UInt8 patch[5];
	const bool ok = mem::Verify(site, expected, sizeof(expected)) &&
	                mem::BuildCallSitePatch(patch, sizeof(patch), site, stub) == sizeof(patch) &&
	                mem::SafeWrite(site, patch, sizeof(patch));
	if (!ok) {
		OBVR_LOG("Wear: the call at %08X is not `call %08X` - %s stays the engine's", site, target, what);
		mem::ReportForeignCode("Wear", site);
	}
	return ok;
}

bool RouteSlot(UInt32 slot, UInt32 expected, UInt32 stub) {
	if (*reinterpret_cast<const UInt32*>(slot) != expected) {
		OBVR_LOG("Wear: the item damage slot %08X holds %08X, not %08X - left alone", slot,
		         *reinterpret_cast<const UInt32*>(slot), expected);
		return false;
	}
	return mem::SafeWrite(slot, &stub, sizeof(stub));
}

}  // namespace

void InstallGearWear() {
	State& s = S();
	static bool s_done = false;
	if (s_done) {
		return;
	}
	s_done = true;
	s.helpersAsRead = mem::Verify(kWornInSlot, kWornInSlotBytes, sizeof(kWornInSlotBytes)) &&
	                  mem::Verify(kContainerChanges, kContainerChangesBytes, sizeof(kContainerChangesBytes)) &&
	                  mem::Verify(kEntryHealth, kEntryHealthBytes, sizeof(kEntryHealthBytes));
	if (!s.helpersAsRead) {
		OBVR_LOG("Wear: the inventory functions are not as read (%s %s %s) - the struck piece and the knock-off stay "
		         "off; the wear's multipliers still apply",
		         mem::Verify(kWornInSlot, kWornInSlotBytes, sizeof(kWornInSlotBytes)) ? "slot ok" : "slot differs",
		         mem::Verify(kContainerChanges, kContainerChangesBytes, sizeof(kContainerChangesBytes)) ? "changes ok" : "changes differ",
		         mem::Verify(kEntryHealth, kEntryHealthBytes, sizeof(kEntryHealthBytes)) ? "health ok" : "health differs");
		return;
	}
	s.pickRouted = RouteCall(kPickArmorCall, kPickArmor, reinterpret_cast<UInt32>(&OnPickArmor), "the worn piece");
	const UInt32 stub = reinterpret_cast<UInt32>(&OnDamageItem);
	if (mem::Verify(kDamageItem, kDamageItemBytes, sizeof(kDamageItemBytes)) &&
	    RouteCall(kDropRecordCall, kDropRecord, reinterpret_cast<UInt32>(&OnDropRecord), "the drop at 0")) {
		s.dropWatched = true;
		const bool c = RouteSlot(kSlotCharacter, kDamageItem, stub);
		const bool k = RouteSlot(kSlotCreature, kDamageItem, stub);
		const bool p = RouteSlot(kSlotPlayer, kPlayerDamageItem, stub);
		s.damageRouted = c && p;
		if (!k) {
			OBVR_LOG("Wear: creatures keep the engine's own zero health");
		}
	}
	OBVR_LOG("Wear: the struck piece %s, a piece at 0 %s", s.pickRouted ? "rerouted" : "the engine's random one",
	         s.damageRouted ? "knocked off by OBVR's rule" : "the engine's");
}

void BeginStrikeWear(void* target, bool parted, BodyPart part, const NiPoint3& blowWay) {
	Strike& st = S().strike;
	st.target = target;
	st.parted = parted;
	st.part = part;
	st.way = blowWay;
}

void EndStrikeWear() {
	S().strike = Strike{};
}

void StepGearWear(const GearWearSettings& settings, float dt, bool inWorld) {
	InstallGearWear();  // once, at the first frame: the game's code is loaded and unpatched by then
	State& s = S();
	s.settings = settings;
	float* const armor = reinterpret_cast<float*>(kArmorWearSetting);
	float* const weapon = reinterpret_cast<float*>(kWeaponWearSetting);
	const float armorWas = s.armorScale.written;
	const float weaponWas = s.weaponScale.written;
	*armor = StepGmstScale(s.armorScale, *armor, settings.armorWearMult);
	*weapon = StepGmstScale(s.weaponScale, *weapon, settings.weaponWearMult);
	if ((*armor != armorWas || *weapon != weaponWas) && s.lines > 0) {
		--s.lines;
		OBVR_LOG("Wear: the engine's settings now armor %.3f (base %.3f), weapon %.4f (base %.4f)",
		         static_cast<double>(*armor), static_cast<double>(s.armorScale.base), static_cast<double>(*weapon),
		         static_cast<double>(s.weaponScale.base));
	}
	if (!inWorld) {
		return;
	}
	for (Knocked& k : s.knocked) {
		if (k.ref == 0) {
			continue;
		}
		if (!RefStillThere(k.print)) {
			k = Knocked{};  // gone with its cell or a load: nothing left to take away
			continue;
		}
		const UInt32 body = k.step.sent ? 0 : RefBodyOf(k.ref);
		const KnockStep step = StepKnockedPiece(k.step, dt, body != 0, settings.knockFlightSeconds);
		if (step.send) {
			SendBodyOff(body, k.velocity);
			SpinBody(body, NiPoint3{3.0f, 5.0f, 2.0f});
		}
		if (step.remove) {
			RequestConsoleLineAs(k.ref, "disable");
			RequestConsoleLineAs(k.ref, "DeleteReference");
			if (s.lines > 0) {
				--s.lines;
				OBVR_LOG("Wear: the knocked-off %08X taken away after %.1f s%s", k.ref, static_cast<double>(k.step.age),
				         k.step.sent ? "" : " - its body never came, it did not fly");
			}
			k = Knocked{};
		}
	}
}

}  // namespace obvr::game
