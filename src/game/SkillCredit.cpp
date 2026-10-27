#include "game/SkillCredit.h"

#include <atomic>

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/GameAddresses.h"
#include "game/SkillCreditLogic.h"
#include "obse/NativeMenuApi.h"

namespace obvr::game {
namespace {

// PluginAPI.h: kInterface_Messaging 4 (added v0017), kMessage_PostLoad 0.
constexpr UInt32 kInterfaceMessaging = 4;
constexpr UInt32 kMessagePostLoad = 0;
constexpr UInt32 kMessagingSinceObse = 17;

obse::MessagingApi* g_messaging = nullptr;
UInt32 g_pluginHandle = 0;

// Replaced from the sender's thread (Dispatch is synchronous), read from the
// strike's: a spin lock round both, held for a copy of at most 1 KB.
SkillExceptions g_exceptions;
std::atomic_flag g_exceptionsLock = ATOMIC_FLAG_INIT;

struct ExceptionsGuard {
	ExceptionsGuard() {
		while (g_exceptionsLock.test_and_set(std::memory_order_acquire)) {
		}
	}
	~ExceptionsGuard() { g_exceptionsLock.clear(std::memory_order_release); }
};

bool g_enabled = true;
UInt32 g_messageLines = 8;
UInt32 g_strikeLines = 12;
bool g_refusalSaid = false;
bool g_patchedSaid = false;

void OnPluginMessage(obse::MessagingApi::Message* message) {
	if (message == nullptr || message->type != kSkillExceptionsMessage) {
		return;
	}
	bool replaced = false;
	UInt32 count = 0;
	{
		ExceptionsGuard guard;
		replaced = g_exceptions.Replace(static_cast<const UInt32*>(message->data), message->dataLen);
		count = g_exceptions.Count();
	}
	if (g_messageLines > 0) {
		--g_messageLines;
		const char* const sender = message->sender != nullptr ? message->sender : "?";
		if (replaced) {
			OBVR_LOG("Skill exceptions: %u base(s) from %s - strikes on them train no skill", count, sender);
		} else {
			OBVR_LOG("Skill exceptions: a message from %s refused - %u bytes are no whole number of ids, %u kept",
			         sender, message->dataLen, count);
		}
	}
}

void OnObseMessage(obse::MessagingApi::Message* message) {
	if (message == nullptr || message->type != kMessagePostLoad || g_messaging == nullptr) {
		return;
	}
	// Every plugin is loaded now: listen to all of them (a null sender adds
	// OBVR to each loaded plugin's listeners, PluginManager::RegisterListener).
	const bool listening = g_messaging->RegisterListener(g_pluginHandle, nullptr, &OnPluginMessage);
	OBVR_LOG("Skill exceptions: %s other plugins' 'SKEX' messages", listening ? "listening for" : "could not listen for");
}

using ThisUInt32Fn = UInt32(__thiscall*)(void* self);
using ThisSetFn = void(__thiscall*)(void* self, UInt32 value);
using UseSkillFn = void(__thiscall*)(void* self, UInt32 skill, UInt32 use, float times);

UInt32 EntryAt(const void* object, UInt32 slotOffset) {
	const UInt32 vtable = *reinterpret_cast<const UInt32*>(object);
	return mem::LooksLikeObjectAddress(vtable) ? *reinterpret_cast<const UInt32*>(vtable + slotOffset) : 0;
}

UInt32 BaseIdOf(const void* ref) {
	const UInt32 r = reinterpret_cast<UInt32>(ref);
	if (!mem::LooksLikeObjectAddress(r)) {
		return 0;
	}
	const UInt32 base = *reinterpret_cast<const UInt32*>(r + addr::kRefBaseFormOffset);
	return mem::LooksLikeObjectAddress(base) ? *reinterpret_cast<const UInt32*>(base + addr::kFormIdOffset) : 0;
}

}  // namespace

void InstallSkillMessages(const obse::Interface* obse) {
	if (obse == nullptr || obse->obseVersion < kMessagingSinceObse || obse->QueryInterface == nullptr ||
	    obse->GetPluginHandle == nullptr) {
		OBVR_LOG("Skill exceptions: no xOBSE messaging - no plugin can set them");
		return;
	}
	g_messaging = static_cast<obse::MessagingApi*>(obse->QueryInterface(kInterfaceMessaging));
	g_pluginHandle = obse->GetPluginHandle();
	if (g_messaging == nullptr || g_messaging->RegisterListener == nullptr ||
	    !g_messaging->RegisterListener(g_pluginHandle, "OBSE", &OnObseMessage)) {
		g_messaging = nullptr;
		OBVR_LOG("Skill exceptions: xOBSE's messaging refused OBVR - no plugin can set them");
	}
}

void SetStrikesTrainSkills(bool enabled) { g_enabled = enabled; }

void CreditStrikeSkill(UInt8* player, void* target) {
	if (!g_enabled || player == nullptr) {
		return;
	}
	// Through the table on purpose: another plugin hooked on a slot sees the
	// call (Oblivion Walkers holds +0x210 and +0x39C). Each slot is the
	// engine's function or one outside the game's code (SlotUsable).
	const UInt32 setPending = EntryAt(player, addr::kActorVtableSetPendingHitsOffset);
	const UInt32 pendingOf = EntryAt(player, addr::kActorVtablePendingHitsOffset);
	const UInt32 skillOf = EntryAt(player, addr::kActorVtableAttackSkillOffset);
	const UInt32 useSkill = EntryAt(player, addr::kActorVtableUseSkillOffset);
	const auto usable = [](UInt32 entry, UInt32 engines) {
		return SlotUsable(entry, engines, addr::kTextStart, addr::kTextEnd);
	};
	if (!usable(setPending, addr::kPlayerSetPendingHits) || !usable(pendingOf, addr::kPlayerPendingHits) ||
	    !usable(skillOf, addr::kPlayerAttackSkill) || !usable(useSkill, addr::kPlayerUseSkill)) {
		if (!g_refusalSaid) {
			g_refusalSaid = true;
			OBVR_LOG("Skill: the player's hit count or skill is not where it was read (%08X %08X %08X, use %08X) "
			         "- strikes train no skill",
			         setPending, pendingOf, skillOf, useSkill);
		}
		return;
	}
	if (!g_patchedSaid && (setPending != addr::kPlayerSetPendingHits || pendingOf != addr::kPlayerPendingHits ||
	                       skillOf != addr::kPlayerAttackSkill || useSkill != addr::kPlayerUseSkill)) {
		g_patchedSaid = true;
		OBVR_LOG("Skill: another plugin holds a slot (count %08X %08X, skill %08X, use %08X) - called through it",
		         setPending, pendingOf, skillOf, useSkill);
	}

	const UInt32 baseId = BaseIdOf(target);
	bool excepted = false;
	{
		ExceptionsGuard guard;
		excepted = g_exceptions.Contains(baseId);
	}
	SkillCreditInput in;
	in.enabled = g_enabled;
	in.pendingHits = reinterpret_cast<ThisUInt32Fn>(pendingOf)(player);
	in.skill = reinterpret_cast<ThisUInt32Fn>(skillOf)(player);
	in.targetExcepted = excepted;
	const SkillCreditPlan plan = PlanSkillCredit(in);
	for (UInt32 i = 0; i < plan.uses; ++i) {
		// use 0 and 0.0 times, as the attack update passes them (0x005FD82B).
		reinterpret_cast<UseSkillFn>(useSkill)(player, in.skill, 0, 0.0f);
	}
	if (plan.clearPending) {
		reinterpret_cast<ThisSetFn>(setPending)(player, 0);
	}
	if (g_strikeLines > 0 && in.pendingHits > 0) {
		--g_strikeLines;
		OBVR_LOG("Skill: a strike on base %08X - skill %u, %u hit(s) counted, %s", baseId, in.skill, in.pendingHits,
		         plan.uses > 0 ? "trained" : (excepted ? "excepted" : "not trained (no skill)"));
	}
}

}  // namespace obvr::game
