#include "game/MenuPause.h"


#include "core/AddressSpace.h"
#include "core/BranchDecode.h"
#include "core/EntryDetour.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "game/MenuMode.h"
#include "game/MenuPausePolicy.h"
#include "game/PlayerControlSites.h"
#include "vr/DialogPanel.h"
#include "vr/TabletLogic.h"

namespace obvr::game {
namespace {

// What keeps the world running, as last applied.
struct PauseSwitches {
	bool unpaused = false;   // [Render] UnpausedMenus
	bool container = false;  // [Look] ContainerInWorld: the container's menu alone
	bool lock = false;       // [Hands] ReachOpens: the lock's minigame too
	bool tablet = false;     // [Tablet] Enabled: the menus held in the hand
	bool Any() const { return unpaused || container || lock || tablet; }
	bool operator==(const PauseSwitches& o) const {
		return unpaused == o.unpaused && container == o.container && lock == o.lock && tablet == o.tablet;
	}
};
PauseSwitches g_switches;
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

// The tablet's share of the answer: its switch on, its menus the ones that
// are up, and the one on top a menu the world runs behind.
bool TabletRunsWorld(bool menuMode, UInt32 top) {
	if (!menuMode || !g_switches.tablet || !vr::TabletMenuRunsWorld(top)) {
		return false;
	}
	UInt32 stack[kMenuStackEntries];
	const UInt32 count = MenuStackEntries(stack, kMenuStackEntries);
	return vr::TabletMenusUp(stack, count);
}

// What the redirected sites call instead of IsMenuMode. Same shape: no
// arguments, the answer in eax, nothing else touched. The vanilla function
// is still asked first, so outside menu mode nothing changes, and with the
// option off the answer is vanilla's own.
int __cdecl WorldPauseForMenu() {
	const bool menuMode = IsMenuMode();
	const bool asking = g_switches.Any();
	const UInt32 observed = menuMode && asking ? TopVisibleMenuId() : kMenuIdNone;
	const UInt32 top = StablePauseMenuId(menuMode, observed, g_rememberedTop);
	g_rememberedTop = top;
	const bool barter = menuMode && asking && BarterMenuUp();
	const bool paused = WorldPausesForMenu(menuMode, g_switches.unpaused, top, g_switches.container, g_switches.lock,
	                                       barter, TabletRunsWorld(menuMode, top));
	if (menuMode && asking &&
	    (!g_lastAnswerLogged || paused != g_lastPaused || top != g_lastTop)) {
		g_lastAnswerLogged = true;
		g_lastPaused = paused;
		g_lastTop = top;
		OBVR_LOG("Menu pause: behind menu %s (0x%03X) the world %s%s", MenuIdName(top), top,
		         paused ? "pauses as vanilla" : "keeps running", barter ? " (a merchant's barter)" : "");
	}
	return paused ? 1 : 0;
}

// The player's controls under a menu: the gate (addr::kPlayerControlsIsMenuModeSite)
// answers "no menu" only while the sticks walk the player under a container's
// menu or the tablet's (SetPlayerWalksUnderMenu); from it to
// addr::kPlayerControlsScopeEndSite the player's reads of the controls and
// the mouse are held back to the walk (PlayerControlReadUnderMenu,
// PlayerControlsReadAxis). Logged on each change of answer.
using ControlStateFn = int(__fastcall*)(void* input, void* unusedEdx, UInt32 control, UInt32 kind);
struct UnderMenu {
	bool walks = false;          // the sticks walk, as the hand mode says
	bool tablet = false;         // and the menus are the tablet's
	bool siteTried = false;      // the gate
	bool siteRedirected = false;
	bool lastRan = false;
	bool wheelTried = false;     // the player's wheel, x and y reads
	bool axesRedirected = false;
	UInt32 wheelLines = 4;
	bool guardTried = false;     // the control reads (GetControlState's entry, the span's end)
	bool guarded = false;
	ControlStateFn original = nullptr;
	bool inSpan = false;         // between the gate and the span's end, the controls run under a menu
	bool spanTablet = false;     // and under the tablet
	UInt32 heldLines = 6;
	UInt32 menuAnimData = 0;     // the player's +0x5DC, taken off for the span (PlayerAnimDataForSpan)
	UInt32 animLines = 2;
};
UnderMenu g_underMenu;

int __cdecl PlayerControlsPauseForMenu() {
	UnderMenu& u = g_underMenu;
	const bool menuMode = IsMenuMode();
	const UInt32 top = menuMode ? TopVisibleMenuId() : kMenuIdNone;
	const bool tablet = menuMode && u.tablet && u.guarded && TabletRunsWorld(menuMode, top);
	const bool run =
		PlayerControlsRunUnderMenu(menuMode, u.walks, top, menuMode && BarterMenuUp(), tablet);
	u.inSpan = menuMode && run;
	u.spanTablet = u.inSpan && tablet;
	// The player's own anim data for its update under the tablet
	// (addr::kPlayerMenuAnimDataOffset): the figure's is put back at the span's end.
	const UInt32 player = u.spanTablet ? *reinterpret_cast<const UInt32*>(addr::kPlayerPointer) : 0;
	UInt32* const figureAnim =
		player != 0 ? reinterpret_cast<UInt32*>(player + addr::kPlayerMenuAnimDataOffset) : nullptr;
	if (figureAnim != nullptr && PlayerOwnAnimDataForSpan(true, *figureAnim != 0)) {
		u.menuAnimData = *figureAnim;
		*figureAnim = 0;
		if (u.animLines > 0) {
			--u.animLines;
			OBVR_LOG("Menu pause: the player walks with its own animation under the tablet - the figure's anim data "
			         "(%08X) set aside for its update",
			         u.menuAnimData);
		}
	}
	if (menuMode && run != u.lastRan) {
		u.lastRan = run;
		OBVR_LOG("Menu pause: the player's controls %s under menu %s (0x%03X)%s",
		         run ? "run - the sticks walk" : "are gated again as vanilla", MenuIdName(top), top,
		         run && tablet ? " - the tablet's, the clicks go on" : "");
	}
	if (!menuMode) {
		u.lastRan = false;
	}
	return run ? 0 : 1;
}

// The span's end (addr::kPlayerControlsScopeEndSite): vanilla's answer.
int __cdecl PlayerControlsSpanEnds() {
	UnderMenu& u = g_underMenu;
	if (u.menuAnimData != 0) {
		const UInt32 player = *reinterpret_cast<const UInt32*>(addr::kPlayerPointer);
		if (player != 0) {
			*reinterpret_cast<UInt32*>(player + addr::kPlayerMenuAnimDataOffset) = u.menuAnimData;
		}
		u.menuAnimData = 0;
	}
	u.inSpan = false;
	u.spanTablet = false;
	return IsMenuMode() ? 1 : 0;
}

// GetControlState's entry: within the span only the walk reads; a held
// back press is logged a few times (the click the player's input would have
// taken as a blow).
int __fastcall PlayerReadsControl(void* input, void* unusedEdx, UInt32 control, UInt32 kind) {
	UnderMenu& u = g_underMenu;
	const int value = u.original(input, unusedEdx, control, kind);
	if (!u.inSpan || PlayerControlReadUnderMenu(control)) {
		return value;
	}
	if (value != 0 && u.heldLines > 0) {
		--u.heldLines;
		OBVR_LOG("Menu pause: control %u (read %u) held back from the player's controls under menu 0x%03X%s", control,
		         kind, TopVisibleMenuId(), control == kControlAttack ? " - the click is the menu's, no attack" : "");
	}
	return 0;
}

// The player's read of a mouse axis (addr::kPlayerWheelReadSite and the x
// and y beside it): the engine's own reader, but 0 where the menu owns the
// axis (PlayerControlsReadAxis). __thiscall with the axis on the stack,
// spelled as __fastcall with a dead edx; the callee pops the one argument.
int __fastcall PlayerReadsMouseAxis(void* input, void* unusedEdx, UInt32 axis) {
	using AxisFn = int(__fastcall*)(void* self, void* edx, UInt32 axis);
	const int value = reinterpret_cast<AxisFn>(addr::kMouseAxisRead)(input, unusedEdx, axis);
	const bool menuMode = IsMenuMode();
	if (PlayerControlsReadAxis(menuMode, axis, menuMode && g_underMenu.spanTablet)) {
		return value;
	}
	if (value != 0 && axis == kMouseAxisWheel && g_underMenu.wheelLines > 0) {
		--g_underMenu.wheelLines;
		OBVR_LOG("Menu pause: the wheel's %d under menu 0x%03X kept from the player's controls - the list scrolls, "
		         "the view stays",
		         value, TopVisibleMenuId());
	}
	return 0;
}

// Points one relative call at `target` when it calls `expected` now; else
// the site is left alone and named in the log.
bool RedirectCall(UInt32 site, UInt32 expected, UInt32 target) {
	mem::RelativeBranch branch;
	if (!mem::DecodeRelativeBranch(reinterpret_cast<const UInt8*>(site), site, branch) || !branch.isCall ||
	    branch.target != expected) {
		mem::ReportForeignCode("Menu pause", site);
		return false;
	}
	const UInt32 rel = target - (site + 5);
	const UInt8 displacement[4] = {
		static_cast<UInt8>(rel), static_cast<UInt8>(rel >> 8), static_cast<UInt8>(rel >> 16),
		static_cast<UInt8>(rel >> 24),
	};
	return mem::SafeWrite(site + 1, displacement, sizeof(displacement));
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

// GetControlState's entry, verified: mov eax,[esp+4]; cmp eax,1Dh.
constexpr UInt8 kControlStateEntry[] = {0x8B, 0x44, 0x24, 0x04, 0x83, 0xF8, 0x1D};

// Detours GetControlState's entry and points the span's end at OBVR's. Both
// or neither: without them a click under a walked tablet would be a blow,
// so the tablet does not walk (UnderMenu::guarded).
bool InstallControlGuard() {
	if (!mem::Verify(addr::kGetControlState, kControlStateEntry, sizeof(kControlStateEntry))) {
		mem::ReportForeignCode("Menu pause", addr::kGetControlState);
		return false;
	}
	constexpr UInt32 kTrampolineCapacity = 16;
	auto* trampoline = static_cast<UInt8*>(mem::AllocExecutable(kTrampolineCapacity));
	if (trampoline == nullptr) {
		return false;
	}
	const UInt32 trampolineAddress = reinterpret_cast<UInt32>(trampoline);
	if (mem::BuildEntryTrampoline(trampoline, kTrampolineCapacity, trampolineAddress, addr::kGetControlState,
	                              kControlStateEntry, sizeof(kControlStateEntry)) == 0) {
		return false;
	}
	UInt8 patch[sizeof(kControlStateEntry)];
	if (mem::BuildEntryPatch(patch, sizeof(patch), addr::kGetControlState,
	                         reinterpret_cast<UInt32>(&PlayerReadsControl), sizeof(patch)) != sizeof(patch)) {
		return false;
	}
	// The span's end first: a detour whose span never ends would hold back
	// every read after the gate, the menus' own too.
	if (!RedirectSite(addr::kPlayerControlsScopeEndSite, reinterpret_cast<UInt32>(&PlayerControlsSpanEnds))) {
		return false;
	}
	g_underMenu.original = reinterpret_cast<ControlStateFn>(trampoline);
	return mem::SafeWrite(addr::kGetControlState, patch, sizeof(patch));
}

}  // namespace

void ApplyUnpausedMenus(bool wanted, bool containerWanted, bool lockWanted, bool tabletWanted) {
	if ((wanted || containerWanted || lockWanted || tabletWanted) && !g_installTried) {
		InstallOnce();
	}
	const bool installed = g_sitesRedirected != 0;
	const PauseSwitches now{wanted && installed, containerWanted && installed, lockWanted && installed,
	                        tabletWanted && installed};
	if (!(now == g_switches)) {
		g_switches = now;
		g_lastAnswerLogged = false;
		if (g_installTried) {
			OBVR_LOG("Menu pause: unpaused menus are now %s, the container's menu %s, the lock's minigame %s, "
			         "the tablet's menus %s",
			         now.unpaused ? "on" : "off", now.container ? "runs the world on its own" : "as the option says",
			         now.lock ? "runs the world on its own" : "pauses as vanilla",
			         now.tablet ? "run the world on their own" : "as the option says");
		}
	}
}

void SetPlayerWalksUnderMenu(bool walking, bool underTablet) {
	UnderMenu& u = g_underMenu;
	if (walking && !u.siteTried) {
		u.siteTried = true;
		u.siteRedirected =
			RedirectSite(addr::kPlayerControlsIsMenuModeSite, reinterpret_cast<UInt32>(&PlayerControlsPauseForMenu));
		OBVR_LOG("Menu pause: the player's controls gate (%08X) %s", addr::kPlayerControlsIsMenuModeSite,
		         u.siteRedirected ? "now asks OBVR - the sticks walk under a reached container's menu or the tablet"
		                          : "keeps its vanilla answer - no walking under a menu");
	}
	if (walking && !u.wheelTried) {
		u.wheelTried = true;
		const UInt32 target = reinterpret_cast<UInt32>(&PlayerReadsMouseAxis);
		const bool wheel = RedirectCall(addr::kPlayerWheelReadSite, addr::kMouseAxisRead, target);
		const bool x = RedirectCall(addr::kPlayerMouseXReadSite, addr::kMouseAxisRead, target);
		const bool y = RedirectCall(addr::kPlayerMouseYReadSite, addr::kMouseAxisRead, target);
		u.axesRedirected = x && y;
		OBVR_LOG("Menu pause: the player's wheel read (%08X) %s; its x and y (%08X, %08X) %s",
		         addr::kPlayerWheelReadSite,
		         wheel ? "now asks OBVR - under a menu the wheel scrolls the list and never the view"
		               : "keeps its vanilla answer - the wheel under a walked menu can zoom the view",
		         addr::kPlayerMouseXReadSite, addr::kPlayerMouseYReadSite,
		         u.axesRedirected ? "too - under the tablet the cursor's steps turn nobody"
		                          : "keep theirs - the tablet does not walk");
	}
	if (walking && !u.guardTried) {
		u.guardTried = true;
		u.guarded = InstallControlGuard();
		OBVR_LOG("Menu pause: the player's control reads (%08X, the span to %08X) %s", addr::kGetControlState,
		         addr::kPlayerControlsScopeEndSite,
		         u.guarded ? "now ask OBVR - under a walked menu only the walk reads, a click is no attack"
		                   : "keep their vanilla answer - the tablet does not walk");
	}
	u.walks = walking && u.siteRedirected;
	u.tablet = underTablet && u.guarded && u.axesRedirected;
}

bool TabletWalkRefused() {
	const UnderMenu& u = g_underMenu;
	return (u.guardTried && !u.guarded) || (u.wheelTried && !u.axesRedirected) ||
	       (u.siteTried && !u.siteRedirected);
}

UInt32 TopVisibleMenu() { return TopVisibleMenuId(); }

bool WorldPausedForMenus() {
	const bool menuMode = IsMenuMode();
	if (!menuMode) {
		return false;
	}
	const UInt32 top = StablePauseMenuId(menuMode, TopVisibleMenuId(), g_rememberedTop);
	return WorldPausesForMenu(menuMode, g_switches.unpaused, top, g_switches.container, g_switches.lock,
	                          BarterMenuUp(), TabletRunsWorld(menuMode, top));
}

bool BarterMenuUp() {
	if (!IsMenuMode()) {
		return false;
	}
	UInt32 stack[kMenuStackEntries];
	const UInt32 count = MenuStackEntries(stack, kMenuStackEntries);
	return BarterInMenuStack(stack, count);
}

bool DialogueMenuUp() {
	if (!IsMenuMode()) {
		return false;
	}
	UInt32 stack[kMenuStackEntries];
	const UInt32 count = MenuStackEntries(stack, kMenuStackEntries);
	return MenuStackHas(stack, count, kMenuIdDialog);
}

bool BarterMenuBegan() {
	static bool s_before = false;
	const bool now = BarterMenuUp();
	const bool began = vr::BarterBegan(s_before, now);
	if (now != s_before) {
		OBVR_LOG("Barter: a merchant's barter %s - %s", now ? "opened over the dialogue" : "closed",
		         now ? "the standard menus' panel where the head looks, the world paused as vanilla"
		             : "the dialogue's own panel again");
	}
	s_before = now;
	return began;
}

UInt32 MenuStackEntries(UInt32* out, UInt32 capacity) {
	const UInt8* const manager = *reinterpret_cast<const UInt8* const*>(addr::kInterfaceManagerPointer);
	if (manager == nullptr || out == nullptr || !mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(manager))) {
		return 0;
	}
	UInt32 count = 0;
	for (UInt32 i = 0; i < addr::kInterfaceMenuStackSlots && count < capacity; ++i) {
		const UInt32 id =
			*reinterpret_cast<const UInt32*>(manager + addr::kInterfaceMenuStackOffset + i * sizeof(UInt32));
		const bool menu = id == kMenuIdBigFour || (id >= kMenuIdFirst && id <= kMenuIdLast);
		if (!menu) {
			break;
		}
		out[count++] = id;
	}
	return count;
}

}  // namespace obvr::game
