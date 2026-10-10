#include "game/MenuPause.h"

#include "core/BranchDecode.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "game/MenuMode.h"
#include "game/MenuPausePolicy.h"

namespace obvr::game {
namespace {

bool g_enabled = false;
bool g_containerEnabled = false;  // [Look] ContainerInWorld: the container's menu alone
bool g_lockEnabled = false;       // [Hands] ReachOpens: the lock's minigame too
bool g_installTried = false;
UInt32 g_sitesRedirected = 0;

// Logged on change of answer, so a run says which menu first kept the world
// running and when the world froze again, and no more.
bool g_lastAnswerLogged = false;
bool g_lastPaused = false;
UInt32 g_lastTop = 0;
UInt32 g_rememberedTop = kMenuIdNone;

UInt32 TopVisibleMenuId() {
	void* manager = *reinterpret_cast<void* const*>(addr::kInterfaceManagerPointer);
	if (manager == nullptr) {
		return kMenuIdNone;
	}
	// __thiscall with no stack arguments, called as __fastcall with a dead
	// edx - the same spelling the scene render hook uses.
	using GetTopFn = UInt32(__fastcall*)(void* self, void* unusedEdx);
	return reinterpret_cast<GetTopFn>(addr::kGetTopVisibleMenuId)(manager, nullptr);
}

// What the redirected sites call instead of IsMenuMode. Same shape: no
// arguments, the answer in eax, nothing else touched. The vanilla function
// is still asked first, so outside menu mode nothing changes, and with the
// option off the answer is vanilla's own.
int __cdecl WorldPauseForMenu() {
	const bool menuMode = IsMenuMode();
	const bool asking = g_enabled || g_containerEnabled || g_lockEnabled;
	const UInt32 observed = menuMode && asking ? TopVisibleMenuId() : kMenuIdNone;
	const UInt32 top = StablePauseMenuId(menuMode, observed, g_rememberedTop);
	g_rememberedTop = top;
	const bool paused = WorldPausesForMenu(menuMode, g_enabled, top, g_containerEnabled, g_lockEnabled);
	if (menuMode && asking &&
	    (!g_lastAnswerLogged || paused != g_lastPaused || top != g_lastTop)) {
		g_lastAnswerLogged = true;
		g_lastPaused = paused;
		g_lastTop = top;
		OBVR_LOG("Menu pause: behind menu %s (0x%03X) the world %s", MenuIdName(top), top,
		         paused ? "pauses as vanilla" : "keeps running");
	}
	return paused ? 1 : 0;
}

// The player's controls gate (addr::kPlayerControlsIsMenuModeSite): "no
// menu" only while the sticks walk the player under the container's menu
// (SetPlayerWalksUnderMenu). Logged on each change of answer.
bool g_walksUnderMenu = false;
bool g_controlsSiteTried = false;
bool g_controlsSiteRedirected = false;
bool g_controlsLastRan = false;

int __cdecl PlayerControlsPauseForMenu() {
	const bool menuMode = IsMenuMode();
	const UInt32 top = menuMode ? TopVisibleMenuId() : kMenuIdNone;
	const bool run = PlayerControlsRunUnderMenu(menuMode, g_walksUnderMenu, top);
	if (menuMode && run != g_controlsLastRan) {
		g_controlsLastRan = run;
		OBVR_LOG("Menu pause: the player's controls %s under menu %s (0x%03X)",
		         run ? "run - the sticks walk" : "are gated again as vanilla", MenuIdName(top), top);
	}
	if (!menuMode) {
		g_controlsLastRan = false;
	}
	return run ? 0 : 1;
}

bool RedirectForeignHook(UInt32 hook, UInt32 target) {
	// Preserve the foreign jump at the game site. Search its short entry stub
	// for the call it makes to IsMenuMode (directly or through the seven-byte
	// import thunk used by ConsoleCommands), and redirect only that call.
	constexpr UInt32 kSearchBytes = 32;
	for (UInt32 offset = 0; offset + 5 <= kSearchBytes; ++offset) {
		const UInt32 callAddress = hook + offset;
		mem::RelativeBranch call;
		if (!mem::DecodeRelativeBranch(reinterpret_cast<const UInt8*>(callAddress),
		                               callAddress, call) || !call.isCall) {
			continue;
		}
		const bool direct = call.target == addr::kIsMenuMode;
		const bool thunk = IsAbsoluteJumpTo(reinterpret_cast<const UInt8*>(call.target),
		                                    addr::kIsMenuMode);
		if (!direct && !thunk) {
			continue;
		}

		const UInt32 rel = target - (callAddress + 5);
		const UInt8 displacement[4] = {
			static_cast<UInt8>(rel), static_cast<UInt8>(rel >> 8),
			static_cast<UInt8>(rel >> 16), static_cast<UInt8>(rel >> 24),
		};
		if (!mem::SafeWrite(callAddress + 1, displacement, sizeof(displacement))) {
			return false;
		}
		OBVR_LOG("Menu pause: joined the existing animation hook at %08X without replacing it",
		         hook);
		return true;
	}
	return false;
}

// Points one `call IsMenuMode` at the policy. Verified first: the site has
// to be a relative call whose target is IsMenuMode, or something else is
// there and the site is left alone and named in the log.
bool RedirectSite(UInt32 site, UInt32 target) {
	const auto* bytes = reinterpret_cast<const UInt8*>(site);
	mem::RelativeBranch branch;
	if (mem::DecodeRelativeBranch(bytes, site, branch) && branch.isJump &&
	    RedirectForeignHook(branch.target, target)) {
		return true;
	}
	if (!mem::DecodeRelativeBranch(bytes, site, branch) || !branch.isCall ||
	    branch.target != addr::kIsMenuMode) {
		OBVR_LOG("Menu pause: the site at %08X is not a call to IsMenuMode, so it keeps "
		         "its vanilla answer",
		         site);
		mem::ReportForeignCode("Menu pause", site);
		return false;
	}
	const UInt32 rel = target - (site + 5);
	const UInt8 displacement[4] = {
		static_cast<UInt8>(rel), static_cast<UInt8>(rel >> 8), static_cast<UInt8>(rel >> 16),
		static_cast<UInt8>(rel >> 24),
	};
	if (!mem::SafeWrite(site + 1, displacement, sizeof(displacement))) {
		OBVR_LOG("Menu pause: the call at %08X could not be rewritten, so it keeps its "
		         "vanilla answer",
		         site);
		return false;
	}
	return true;
}

void InstallOnce() {
	if (g_installTried) {
		return;
	}
	g_installTried = true;
	constexpr UInt32 kSiteCount =
		sizeof(addr::kUpdateStepIsMenuModeSites) / sizeof(addr::kUpdateStepIsMenuModeSites[0]);
	for (UInt32 i = 0; i < kSiteCount; ++i) {
		if (RedirectSite(addr::kUpdateStepIsMenuModeSites[i], reinterpret_cast<UInt32>(&WorldPauseForMenu))) {
			++g_sitesRedirected;
		}
	}
	OBVR_LOG("Menu pause: %u of %u update-step sites now ask OBVR whether to pause - the "
	         "world keeps running behind the inventory, magic, map, stats, container and "
	         "book menus, and pauses behind every other menu as vanilla does",
	         g_sitesRedirected, kSiteCount);
}

}  // namespace

void ApplyUnpausedMenus(bool wanted, bool containerWanted, bool lockWanted) {
	if ((wanted || containerWanted || lockWanted) && !g_installTried) {
		InstallOnce();
	}
	const bool enabled = wanted && g_sitesRedirected != 0;
	const bool containerEnabled = containerWanted && g_sitesRedirected != 0;
	const bool lockEnabled = lockWanted && g_sitesRedirected != 0;
	if (enabled != g_enabled || containerEnabled != g_containerEnabled || lockEnabled != g_lockEnabled) {
		g_enabled = enabled;
		g_containerEnabled = containerEnabled;
		g_lockEnabled = lockEnabled;
		g_lastAnswerLogged = false;
		if (g_installTried) {
			OBVR_LOG("Menu pause: unpaused menus are now %s, the container's menu %s, the lock's minigame %s",
			         enabled ? "on" : "off", containerEnabled ? "runs the world on its own" : "as the option says",
			         lockEnabled ? "runs the world on its own" : "pauses as vanilla");
		}
	}
}

void SetPlayerWalksUnderMenu(bool walking) {
	if (walking && !g_controlsSiteTried) {
		g_controlsSiteTried = true;
		g_controlsSiteRedirected =
			RedirectSite(addr::kPlayerControlsIsMenuModeSite, reinterpret_cast<UInt32>(&PlayerControlsPauseForMenu));
		OBVR_LOG("Menu pause: the player's controls gate (%08X) %s", addr::kPlayerControlsIsMenuModeSite,
		         g_controlsSiteRedirected ? "now asks OBVR - the sticks walk under a container's menu opened by reaching"
		                                  : "keeps its vanilla answer - no walking under a menu");
	}
	g_walksUnderMenu = walking && g_controlsSiteRedirected;
}

UInt32 TopVisibleMenu() { return TopVisibleMenuId(); }

}  // namespace obvr::game
