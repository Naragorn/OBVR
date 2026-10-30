#include "game/WalkersLink.h"

#include <atomic>
#include <cstring>

#include "core/Log.h"

namespace obvr::game {
namespace {

obse::MessagingApi* g_messaging = nullptr;
UInt32 g_pluginHandle = 0;

// Written on the sender's thread (Dispatch is synchronous), read by the
// frame: a spin lock round both, held for a copy of a few words.
WalkersNow g_now;
std::atomic_flag g_lock = ATOMIC_FLAG_INIT;
UInt32 g_testArm = 0;
UInt32 g_lines = 24;
UInt32 g_vrLines = 2;

struct Guard {
	Guard() {
		while (g_lock.test_and_set(std::memory_order_acquire)) {
		}
	}
	~Guard() { g_lock.clear(std::memory_order_release); }
};

bool FromWalkers(const char* sender) {
	return sender != nullptr && std::strcmp(sender, vr::kWalkersPluginName) == 0;
}

}  // namespace

void SetWalkersMessaging(obse::MessagingApi* messaging, UInt32 pluginHandle) {
	g_messaging = messaging;
	g_pluginHandle = pluginHandle;
}

bool SayVrToWalkers() {
	if (g_messaging == nullptr || g_messaging->Dispatch == nullptr) {
		return false;
	}
	{
		Guard guard;
		if (g_now.linked) {
			return true;
		}
	}
	UInt32 vr = 1;
	const bool sent =
		g_messaging->Dispatch(g_pluginHandle, vr::kWalkersVrMessage, &vr, sizeof(vr), vr::kWalkersPluginName);
	{
		Guard guard;
		g_now.linked = sent;
	}
	if (sent || g_vrLines > 0) {
		g_vrLines -= g_vrLines > 0 ? 1 : 0;
		OBVR_LOG("Walkers: 'WGVR' 1 %s - %s", sent ? "taken" : "not taken",
		         sent ? "the grab is shown on the body, shaken off by the held hand"
		              : "no Oblivion Walkers listening to OBVR, its grab stays flat");
	}
	return sent;
}

void OnWalkersMessage(const char* sender, UInt32 type, const void* data, UInt32 bytes) {
	if (type != vr::kWalkersHoldsMessage && type != vr::kWalkersBiteMessage) {
		return;
	}
	if (!FromWalkers(sender)) {
		return;
	}
	bool taken = false;
	WalkersNow copy;
	{
		Guard guard;
		if (type == vr::kWalkersHoldsMessage) {
			taken = vr::ParseWalkersHolds(data, bytes, g_now.holds);
			g_now.holdsSerial += taken ? 1 : 0;
		} else {
			taken = vr::ParseWalkersBite(data, bytes, g_now.bite);
			g_now.biteSerial += taken ? 1 : 0;
		}
		copy = g_now;
	}
	if (g_lines > 0) {
		--g_lines;
		if (!taken) {
			OBVR_LOG("Walkers: a %s message of %u bytes refused", type == vr::kWalkersHoldsMessage ? "'WGRB'" : "'WGBT'",
			         bytes);
		} else if (type == vr::kWalkersHoldsMessage) {
			OBVR_LOG("Walkers: %u holder(s) - left arm %08X, right arm %08X", copy.holds.count,
			         vr::HolderOf(copy.holds, vr::kWalkersArmLeft), vr::HolderOf(copy.holds, vr::kWalkersArmRight));
		} else {
			OBVR_LOG("Walkers: a bite at the %s arm - %u of %u%s", copy.bite.arm == vr::kWalkersArmLeft ? "left" : "right",
			         copy.bite.number, copy.bite.toDie, copy.bite.lethal ? ", the last" : "");
		}
	}
}

WalkersNow ReadWalkersNow() {
	WalkersNow copy;
	{
		Guard guard;
		copy = g_now;
	}
	if (g_testArm != 0 && copy.holds.count == 0) {
		copy.holds.count = 1;
		copy.holds.refId[0] = 0xFFFFFFFFu;
		copy.holds.arm[0] = g_testArm;
	}
	return copy;
}

bool SendWalkersStroke(UInt32 arm) {
	if (g_messaging == nullptr || g_messaging->Dispatch == nullptr) {
		return false;
	}
	UInt32 word = arm;
	return g_messaging->Dispatch(g_pluginHandle, vr::kWalkersShakeMessage, &word, sizeof(word),
	                             vr::kWalkersPluginName);
}

void SetWalkersTestHold(UInt32 arm) {
	g_testArm = arm == vr::kWalkersArmLeft || arm == vr::kWalkersArmRight ? arm : 0;
}

}  // namespace obvr::game
