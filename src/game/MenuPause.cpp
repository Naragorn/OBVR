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
	const UInt32 observed = menuMode && g_enabled ? TopVisibleMenuId() : kMenuIdNone;
	const UInt32 top = StablePauseMenuId(menuMode, observed, g_rememberedTop);
	g_rememberedTop = top;
	const bool paused = WorldPausesForMenu(menuMode, g_enabled, top);
	if (menuMode && g_enabled &&
	    (!g_lastAnswerLogged || paused != g_lastPaused || top != g_lastTop)) {
		g_lastAnswerLogged = true;
		g_lastPaused = paused;
		g_lastTop = top;
		OBVR_LOG("Menu pause: behind menu %s (0x%03X) the world %s", MenuIdName(top), top,
		         paused ? "pauses as vanilla" : "keeps running");
	}
	return paused ? 1 : 0;
}

bool RedirectForeignHook(UInt32 hook) {
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

		const UInt32 target = reinterpret_cast<UInt32>(&WorldPauseForMenu);
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
bool RedirectSite(UInt32 site) {
	const auto* bytes = reinterpret_cast<const UInt8*>(site);
	mem::RelativeBranch branch;
	if (mem::DecodeRelativeBranch(bytes, site, branch) && branch.isJump &&
	    RedirectForeignHook(branch.target)) {
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
	const UInt32 target = reinterpret_cast<UInt32>(&WorldPauseForMenu);
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
		if (RedirectSite(addr::kUpdateStepIsMenuModeSites[i])) {
			++g_sitesRedirected;
		}
	}
	OBVR_LOG("Menu pause: %u of %u update-step sites now ask OBVR whether to pause - the "
	         "world keeps running behind the inventory, magic, map, stats, container and "
	         "book menus, and pauses behind every other menu as vanilla does",
	         g_sitesRedirected, kSiteCount);
}

}  // namespace

void ApplyUnpausedMenus(bool wanted) {
	if (wanted && !g_installTried) {
		InstallOnce();
	}
	const bool enabled = wanted && g_sitesRedirected != 0;
	if (enabled != g_enabled) {
		g_enabled = enabled;
		g_lastAnswerLogged = false;
		if (g_installTried) {
			OBVR_LOG("Menu pause: unpaused menus are now %s", enabled ? "on" : "off");
		}
	}
}

}  // namespace obvr::game
