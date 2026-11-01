#include "camera/hands/Rings.h"

#include <cstdio>

#include "core/Log.h"
#include "game/ItemIcons.h"
#include "game/MenuType.h"
#include "game/QuickKeyPages.h"
#include "game/SpellBook.h"
#include "platform/PluginPath.h"
#include "render/GameDevice.h"
#include "test/HandScriptRuntime.h"
#include "vr/MenuHaptics.h"

namespace obvr::camera::hands {
namespace {

static_assert(ui::kQuickMenuIconSide == game::kItemIconSide, "the ring draws the icons one to one");
static_assert(vr::kQuickMenuMaxPages == game::kQuickKeyPagesMax, "one page count for the ring and the store");
static_assert(vr::kSpellRingSlots == vr::kQuickSlots, "the spell ring is the quick menu's ring");

// The ring upright where the hand opened it, facing the head: its right the
// ring's right, its up tracking up, its face towards the eyes; a few
// centimetres ahead of the hand so the controller does not hide the middle.
vr::openvr::HmdMatrix34 QuickMenuPose(const vr::QuickMenuState& s) {
	const NiPoint3 toHead{-s.right.z, 0.0f, s.right.x};
	const NiPoint3 at = s.anchor - toHead * 0.03f;
	vr::openvr::HmdMatrix34 pose{};
	const NiPoint3 axes[3] = {s.right, s.up, toHead};
	for (int column = 0; column < 3; ++column) {
		pose.m[0][column] = axes[column].x;
		pose.m[1][column] = axes[column].y;
		pose.m[2][column] = axes[column].z;
	}
	pose.m[0][3] = at.x;
	pose.m[1][3] = at.y;
	pose.m[2][3] = at.z;
	return pose;
}

}  // namespace

// `assign`: the cursor is on the inventory or the magic menu, where the ring
// sets a hotkey to what the cursor is on (docs/controls-spec.md 4.5).
void UpdateQuickMenu(RingsState& state, const HandStep& step, const HandShared& shared, bool allowed, bool assign,
                     const vr::HandModeFrame& frame) {
	const Config& config = step.config;
	vr::OpenVRBackend& backend = step.backend;
	const float dt = step.dt;
	vr::HandModeResult& hand = shared.hand;
	vr::QuickMenuInput in;
	// The right trackpad held, or the right stick's short click as the tap
	// that latches the ring open (vr::StepStickHold, the hand mode's ringTap)
	// - the Oculus Touch has no trackpad.
	in.pad = frame.right.valid && vr::TrackpadClickDown(frame.right.buttonsPressed);
	in.tap = hand.ringTap;
	in.allowed = allowed;
	in.assign = assign;
	in.hand = frame.right.position;
	in.headRight = frame.headValid ? vr::ToMatrix(frame.head) * NiPoint3{1.0f, 0.0f, 0.0f}
	                               : NiPoint3{1.0f, 0.0f, 0.0f};
	in.dt = dt;
	in.trigger = state.quickMenuTrigger;
	// The pages: several with the co-save to keep them in, the game's eight
	// alone without. A ring opening on a page past the count (the count
	// lowered since) turns to the first.
	// As many pages as hold hotkeys and one empty after them (game::
	// CountQuickKeyPages); 1 without the co-save.
	const int pageCount = vr::QuickMenuPageCount(game::QuickKeyPageCountNow());
	if (in.pad && !state.quickMenu.open && allowed && game::CurrentQuickKeyPage() >= pageCount) {
		game::TurnQuickKeyPage(pageCount, true);
	}
	// The hotkeys are read while the trackpad is down or the ring is open:
	// what the ring shows and what a release uses is what they hold now.
	if (in.pad || state.quickMenu.open) {
		game::QuickKeySlot slots[game::kQuickKeyCount];
		game::ReadQuickKeys(slots);
		for (int i = 0; i < game::kQuickKeyCount; ++i) {
			in.filled[i] = slots[i].filled;
			const char* const iconPath = config.hands.quickMenu.icons ? slots[i].iconPath : "";
			if (state.quickMenuView.filled[i] != slots[i].filled ||
			    std::strcmp(state.quickMenuView.names[i], slots[i].name) != 0 ||
			    std::strcmp(state.quickMenuIconPaths[i], iconPath) != 0) {
				state.quickMenuView.filled[i] = slots[i].filled;
				strncpy_s(state.quickMenuView.names[i], slots[i].name, _TRUNCATE);
				strncpy_s(state.quickMenuIconPaths[i], iconPath, _TRUNCATE);
				++state.quickMenuRevision;
			}
		}
		// Asked for every time: the cache keeps what was asked for last, so
		// the eight shown are never the ones it lets go (game/ItemIcons.h).
		for (int i = 0; i < game::kQuickKeyCount; ++i) {
			const render::Pixel* const icon =
				state.quickMenuView.filled[i] ? game::ItemIcon(state.quickMenuIconPaths[i]) : nullptr;
			if (icon != state.quickMenuView.icons[i]) {
				state.quickMenuView.icons[i] = icon;
				++state.quickMenuRevision;
			}
		}
	}
	vr::QuickMenuSettings settings = config.hands.quickMenu;
	settings.pages = pageCount;
	const vr::QuickMenuVerdict v = vr::StepQuickMenu(state.quickMenu, in, settings);
	// The tick at the ring's slots (vr/MenuHaptics.h), in the trackpad's hand.
	shared.menuPulse(true, vr::StepRowHaptic(state.ringHaptic, v.visible, v.highlighted, v.used >= 0));
	if (v.turnPage) {
		// The game's eight are written now; the next frame's read shows them.
		game::TurnQuickKeyPage(pageCount);
	}
	const int page = game::CurrentQuickKeyPage();
	if (page != state.quickMenuView.page || pageCount != state.quickMenuView.pageCount) {
		state.quickMenuView.page = page;
		state.quickMenuView.pageCount = pageCount;
		++state.quickMenuRevision;
	}
	hand.controls.quickKey = static_cast<UInt8>(v.key);
	hand.controls.menuClick = hand.controls.menuClick || v.click;
	if (v.holdsCursor) {
		// The click has to land on what the laser was on when the trackpad
		// went down, not where the hand has moved the laser to reach a slot:
		// neither the mouse steps nor the direct placement (LaserCursorDirect,
		// which walked the cursor to the slot since 2026-10-10 - the tester,
		// 2026-10-15: "laserpointer sich wegbewegt vom item").
		hand.cursorDx = 0;
		hand.cursorDy = 0;
		hand.cursorWanted = false;
	}
	if (v.highlighted != state.quickMenuView.highlighted || v.assigning != state.quickMenuView.assigning) {
		state.quickMenuView.highlighted = v.highlighted;
		state.quickMenuView.assigning = v.assigning;
		++state.quickMenuRevision;
	}
	if (v.assigned >= 0) {
		OBVR_LOG("QuickMenu: setting hotkey %d to what the cursor is on in the %s menu (%s) - "
		         "its number key held, then a click",
		         v.assigned + 1, game::MenuIdName(game::ActiveMenuId()),
		         state.quickMenuView.filled[v.assigned] ? state.quickMenuView.names[v.assigned]
		                                            : "empty until now");
	}
	if (v.assignDone || v.assignAborted) {
		game::QuickKeySlot slots[game::kQuickKeyCount];
		game::ReadQuickKeys(slots);
		char list[400] = "";
		for (int i = 0; i < game::kQuickKeyCount; ++i) {
			char one[56];
			std::snprintf(one, sizeof(one), "%s%d %s", i == 0 ? "" : ", ", i + 1,
			              slots[i].filled ? (slots[i].name[0] != '\0' ? slots[i].name : "(no name)")
			                              : "-");
			strncat_s(list, one, _TRUNCATE);
		}
		OBVR_LOG("QuickMenu: setting a hotkey %s - the hotkeys now: %s",
		         v.assignDone ? "done" : "stopped, the menu went", list);
	}
	if (v.opened) {
		state.quickMenuPose = QuickMenuPose(state.quickMenu);
		if (state.quickMenuLinesLeft > 0) {
			--state.quickMenuLinesLeft;
			char list[400] = "";
			for (int i = 0; i < game::kQuickKeyCount; ++i) {
				char one[56];
				std::snprintf(one, sizeof(one), "%s%d %s", i == 0 ? "" : ", ", i + 1,
				              state.quickMenuView.filled[i]
				                  ? (state.quickMenuView.names[i][0] != '\0' ? state.quickMenuView.names[i]
				                                                          : "(no name)")
				                  : "-");
				strncat_s(list, one, _TRUNCATE);
			}
			OBVR_LOG("QuickMenu: opened at the hand (%.2f %.2f %.2f) - %s",
			         static_cast<double>(state.quickMenu.anchor.x),
			         static_cast<double>(state.quickMenu.anchor.y),
			         static_cast<double>(state.quickMenu.anchor.z), list);
			char raw[400];
			game::DescribeQuickKeyLists(raw, sizeof(raw));
			OBVR_LOG("QuickMenu: the lists (start node/count->form) %s", raw);
		}
	}
	if ((v.used >= 0 || v.cancelled) && state.quickMenuLinesLeft > 0) {
		--state.quickMenuLinesLeft;
		if (v.used >= 0) {
			OBVR_LOG("QuickMenu: hotkey %d used (%s) - its number key tapped", v.used + 1,
			         state.quickMenuView.names[v.used]);
		} else {
			OBVR_LOG("QuickMenu: closed without a hotkey");
		}
	}
	if (v.visible) {
		state.quickMenuLayer.Show(backend, render::GetGameDevice(), state.quickMenuPose,
		                      ui::QuickMenuWidthMetres(config.hands.quickMenu.ringMetres),
		                      state.quickMenuRevision, ui::PaintQuickMenuFor, &state.quickMenuView);
	} else {
		state.quickMenuLayer.Hide(backend);
	}
	// For the test runner: the ring as painted, at each mark it is up for.
	if (test::HandScriptMarkedThisFrame() && state.quickMenuLayer.IsVisible()) {
		char name[96];
		char path[512];
		std::snprintf(name, sizeof(name), "OBVR-QuickMenu-%s.bmp", test::HandScriptMarkName());
		if (platform::BuildGamePath(name, path, sizeof(path)) && state.quickMenuLayer.SaveBmp(path)) {
			OBVR_LOG("HandScript: the quick menu's picture saved as %s", name);
		}
	}
}

namespace {

// The spell ring's view for the page shown: the spells' names and icons,
// the active spell's mark, the page dots. The revision moves when anything
// the picture shows has changed.
void RefreshSpellRingView(RingsState& state, const Config& config, int highlighted) {
	const int pageCount = vr::SpellRingPageCount(state.spellBook.count);
	bool changed = false;
	for (int i = 0; i < vr::kSpellRingSlots; ++i) {
		const int index = vr::SpellRingIndex(state.spellRingPage, i, state.spellBook.count);
		const bool filled = index >= 0;
		const char* const name = filled ? state.spellBook.spells[index].name : "";
		const char* const iconPath = filled && config.hands.quickMenu.icons ? state.spellBook.spells[index].iconPath : "";
		if (state.spellRingView.filled[i] != filled || std::strcmp(state.spellRingView.names[i], name) != 0 ||
		    std::strcmp(state.spellRingIconPaths[i], iconPath) != 0) {
			state.spellRingView.filled[i] = filled;
			strncpy_s(state.spellRingView.names[i], name, _TRUNCATE);
			strncpy_s(state.spellRingIconPaths[i], iconPath, _TRUNCATE);
			changed = true;
		}
	}
	// Asked for every time, as the quick menu's are: the cache keeps what
	// was asked for last (game/ItemIcons.h).
	for (int i = 0; i < vr::kSpellRingSlots; ++i) {
		const render::Pixel* const icon =
			state.spellRingView.filled[i] && state.spellRingIconPaths[i][0] != '\0' ? game::ItemIcon(state.spellRingIconPaths[i]) : nullptr;
		if (icon != state.spellRingView.icons[i]) {
			state.spellRingView.icons[i] = icon;
			changed = true;
		}
	}
	const int active = vr::SpellRingSlotOf(state.spellBook, state.spellRingPage, game::ActiveSpellForm(state.spellBook));
	if (state.spellRingView.active != active || state.spellRingView.highlighted != highlighted ||
	    state.spellRingView.page != state.spellRingPage || state.spellRingView.pageCount != pageCount ||
	    state.spellRingView.title[0] == '\0') {
		state.spellRingView.active = active;
		state.spellRingView.highlighted = highlighted;
		state.spellRingView.page = state.spellRingPage;
		state.spellRingView.pageCount = pageCount;
		strncpy_s(state.spellRingView.title, "Spells", _TRUNCATE);
		changed = true;
	}
	if (changed) {
		++state.spellRingRevision;
	}
}

// The page's spells for the log: "1 Flare, 2 -, ...".
void DescribeSpellRingPage(const RingsState& state, char* out, UInt32 size) {
	out[0] = '\0';
	for (int i = 0; i < vr::kSpellRingSlots; ++i) {
		const int index = vr::SpellRingIndex(state.spellRingPage, i, state.spellBook.count);
		char one[56];
		std::snprintf(one, sizeof(one), "%s%d %s", i == 0 ? "" : ", ", i + 1,
		              index >= 0 ? (state.spellBook.spells[index].name[0] != '\0' ? state.spellBook.spells[index].name : "(no name)")
		                         : "-");
		strncat_s(out, size, one, _TRUNCATE);
	}
}

}  // namespace

// `allowed`: Full VR on, in the world, no menu up, the quick menu shut.
void UpdateSpellRing(RingsState& state, const HandStep& step, const HandShared& shared, bool allowed,
                     const vr::HandModeFrame& frame) {
	const Config& config = step.config;
	vr::OpenVRBackend& backend = step.backend;
	const float dt = step.dt;
	vr::HandModeResult& hand = shared.hand;
	vr::QuickMenuInput in;
	// The other stick's short click, or on an Index the other trackpad held
	// (the hand mode's spellRingPad), as the right trackpad is the quick menu's.
	in.pad = hand.spellRingPad;
	in.tap = hand.spellRingTap;
	in.allowed = allowed;
	in.hand = frame.left.position;
	in.headRight = frame.headValid ? vr::ToMatrix(frame.head) * NiPoint3{1.0f, 0.0f, 0.0f}
	                               : NiPoint3{1.0f, 0.0f, 0.0f};
	in.dt = dt;
	in.trigger = state.spellRingTrigger;
	// The book is read as the ring opens: what it shows is what the player
	// knows now, in the order of the names.
	if ((in.tap || in.pad) && !state.spellRing.open && allowed && config.hands.spellRing) {
		UInt32 leftOut = 0;
		const bool read = game::ReadPlayerSpells(state.spellBook, leftOut);
		if ((!read || leftOut > 0) && state.spellRingLinesLeft > 0) {
			--state.spellRingLinesLeft;
			OBVR_LOG("SpellRing: %s", !read ? "the player's base form could not be read - the ring is empty"
			                                : "more spells than the ring's ten pages hold");
			if (leftOut > 0) {
				OBVR_LOG("SpellRing: %u castable spell(s) past the eightieth left out", leftOut);
			}
		}
	}
	vr::QuickMenuSettings settings = config.hands.quickMenu;
	settings.enabled = config.hands.spellRing;
	const int pageCount = vr::SpellRingPageCount(state.spellBook.count);
	// The ring's own step reads the page count only as "more than one".
	settings.pages = pageCount > vr::kQuickMenuMaxPages ? vr::kQuickMenuMaxPages : pageCount;
	state.spellRingPage = vr::SpellRingPageClamp(state.spellRingPage, pageCount);
	for (int i = 0; i < vr::kSpellRingSlots; ++i) {
		in.filled[i] = vr::SpellRingIndex(state.spellRingPage, i, state.spellBook.count) >= 0;
	}
	const vr::QuickMenuVerdict v = vr::StepQuickMenu(state.spellRing, in, settings);
	// The tick at the ring's slots (vr/MenuHaptics.h), in the other hand.
	shared.menuPulse(false, vr::StepRowHaptic(state.spellRingHaptic, v.visible, v.highlighted, v.used >= 0));
	if (v.turnPage) {
		state.spellRingPage = vr::SpellRingNextPage(state.spellRingPage, pageCount);
	}
	if (v.opened) {
		state.spellRingPose = QuickMenuPose(state.spellRing);
		if (state.spellRingLinesLeft > 0) {
			--state.spellRingLinesLeft;
			char list[400];
			DescribeSpellRingPage(state, list, sizeof(list));
			OBVR_LOG("SpellRing: opened at the other hand (%.2f %.2f %.2f) - %d spell(s) known, page %d of %d: %s",
			         static_cast<double>(state.spellRing.anchor.x), static_cast<double>(state.spellRing.anchor.y),
			         static_cast<double>(state.spellRing.anchor.z), state.spellBook.count, state.spellRingPage + 1, pageCount,
			         list);
			char raw[400];
			game::DescribeSpellList(raw, sizeof(raw));
			OBVR_LOG("SpellRing: the list (node:form(id name)/formType/spellType/cost/level) %s", raw);
		}
	}
	if (v.used >= 0) {
		const int index = vr::SpellRingIndex(state.spellRingPage, v.used, state.spellBook.count);
		const UInt32 form = index >= 0 ? state.spellBook.spells[index].form : 0;
		const bool took = form != 0 && game::MakeActiveSpell(form);
		const UInt32 now = game::ActiveSpellForm(state.spellBook);
		if (state.spellRingLinesLeft > 0) {
			--state.spellRingLinesLeft;
			char nowName[40] = "";
			for (int i = 0; i < state.spellBook.count; ++i) {
				if (state.spellBook.spells[i].form == now) {
					strncpy_s(nowName, state.spellBook.spells[i].name, _TRUNCATE);
				}
			}
			OBVR_LOG("SpellRing: slot %d used (%s, %08X) - %s; the active spell now %s (%08X)", v.used + 1,
			         index >= 0 ? state.spellBook.spells[index].name : "-", form,
			         took ? "the engine took it" : "the engine refused it", now != 0 ? nowName : "none", now);
		}
	} else if (v.cancelled && state.spellRingLinesLeft > 0) {
		--state.spellRingLinesLeft;
		OBVR_LOG("SpellRing: closed without a spell");
	}
	if (v.visible) {
		RefreshSpellRingView(state, config, v.highlighted);
		state.spellRingLayer.Show(backend, render::GetGameDevice(), state.spellRingPose,
		                      ui::QuickMenuWidthMetres(config.hands.quickMenu.ringMetres), state.spellRingRevision,
		                      ui::PaintQuickMenuFor, &state.spellRingView);
	} else {
		state.spellRingLayer.Hide(backend);
	}
	// For the test runner: the ring as painted, at each mark it is up for.
	if (test::HandScriptMarkedThisFrame() && state.spellRingLayer.IsVisible()) {
		char name[96];
		char path[512];
		std::snprintf(name, sizeof(name), "OBVR-SpellRing-%s.bmp", test::HandScriptMarkName());
		if (platform::BuildGamePath(name, path, sizeof(path)) && state.spellRingLayer.SaveBmp(path)) {
			OBVR_LOG("HandScript: the spell ring's picture saved as %s", name);
		}
	}
}

}  // namespace obvr::camera::hands
