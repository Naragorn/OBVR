#pragma once

// The runner's work OBVR does itself, so a run needs no keystrokes from
// outside the game: the script's console lines, and loading the run's save
// from the main menu. The Windows runner (tools/hand-script-run.ps1) typed
// both into the game by scan code; under Proton that route depends on window
// focus and the desktop's input tools (docs/linux.md, gate 3), so the Linux
// runner switches these on instead:
//
//   [Debug] HandScriptConsole=1   OBVR hands the script's `console` lines to
//                                 the game once the script has started, and
//                                 its `console-at <mark>` lines once that
//                                 mark is written - one line a frame, through
//                                 xOBSE's console interface (game/ConsoleLine)
//   [Debug] HandScriptLoad=<save> OBVR asks the main menu's console for
//                                 "load <save>" once the main menu has stood
//                                 for a moment
//
// Pure, covered by hand_script_test.

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "core/Types.h"
#include "test/HandScript.h"

namespace obvr::test {

// The longest line game/ConsoleLine takes (its buffers hold 96 characters).
inline constexpr size_t kScriptConsoleLineMost = 95;
// Frames a refused line is asked for again before it is given up: the
// console's queue takes four lines a frame and empties every frame, so a
// line refused this long is refused for good (no console interface).
inline constexpr UInt32 kScriptConsoleRefusalsMost = 120;
// Seconds of game time the start's lines wait after the script has started.
// Handed over in the first frames after the load, `player.drop` left the
// dagger of activate-takes 86 units under the player's feet (the Linux run of
// 2026-10-14, the "HandScript: items" line: the item at z -252, the eyes at
// -54, 112 units above the feet), where the hand could not reach it; the
// Windows runner's typing had given the world these seconds. Every scenario
// with start lines waits at least ten seconds first.
inline constexpr float kScriptConsoleStartSettleSeconds = 4.0f;

// A plugin's form ID base by its file name: a regular plugin's load index
// shifted up 24 bits, or a light plugin's FE000000 | its light index << 12
// (OblivionESL's ESL_GetFormIDBase). False when no plugin of that name is
// loaded. The engine side is test/HandScriptRuntime.cpp's.
using ScriptFormResolver = std::function<bool(const std::string& plugin, UInt32& base, bool& light)>;

// The form ID of a plugin's local ID: a light plugin keeps twelve bits of it,
// a regular one twenty-four (Oblivion Walkers' RiseLogic.h EspFormId, in the
// game 2026-10-28).
inline UInt32 FormIdOfPlugin(UInt32 base, bool light, UInt32 local) {
	return base | (local & (light ? 0x00000FFFu : 0x00FFFFFFu));
}

// "{form:<plugin>:<local hex>}" at `at`: its length, the plugin and the local
// ID; 0 when the text there is no whole token.
inline size_t ParseFormToken(const std::string& line, size_t at, std::string& plugin, UInt32& local) {
	if (line.compare(at, 6, "{form:") != 0) {
		return 0;
	}
	const size_t close = line.find('}', at);
	if (close == std::string::npos) {
		return 0;
	}
	const std::string body = line.substr(at + 6, close - at - 6);
	const size_t colon = body.rfind(':');
	if (colon == std::string::npos || colon == 0 || colon + 1 >= body.size() || body.size() - colon - 1 > 8) {
		return 0;
	}
	UInt32 value = 0;
	for (size_t i = colon + 1; i < body.size(); ++i) {
		const char c = body[i];
		const UInt32 digit = c >= '0' && c <= '9'   ? static_cast<UInt32>(c - '0')
		                     : c >= 'a' && c <= 'f' ? static_cast<UInt32>(c - 'a' + 10)
		                     : c >= 'A' && c <= 'F' ? static_cast<UInt32>(c - 'A' + 10)
		                                            : 16u;
		if (digit > 15) {
			return 0;
		}
		value = (value << 4) | digit;
	}
	plugin = body.substr(0, colon);
	local = value;
	return close - at + 1;
}

// A console line as the runner typed it: {weapon} becomes the equipped
// weapon's form ID from the script's start, {near} the form ID of the item
// nearest the eyes from the mark's "HandScript: items" line, {actor} the
// form ID of the actor nearest the player from the mark's "HandScript:
// actors" line (game/ActorRosterLogic.h) - all as eight hex digits, as those
// lines print them. {form:<plugin>:<local hex>} becomes that plugin's form
// (Walkers' proposal with Nadi's okay, 2026-10-28: since Linux has Nadi's
// Windows mod list, scenarios cannot rely on a load index); one that cannot
// be resolved - no resolver, the plugin not loaded, no hex ID, no closing
// brace - is left as written and `unresolved` set.
inline std::string ExpandScriptConsoleLine(const std::string& line, UInt32 weaponForm, UInt32 nearForm,
                                           UInt32 actorForm, const ScriptFormResolver* resolve = nullptr,
                                           bool* unresolved = nullptr) {
	char weapon[16];
	char near_[16];
	char actor[16];
	std::snprintf(weapon, sizeof(weapon), "%08X", weaponForm);
	std::snprintf(near_, sizeof(near_), "%08X", nearForm);
	std::snprintf(actor, sizeof(actor), "%08X", actorForm);
	std::string out;
	out.reserve(line.size() + 16);
	for (size_t i = 0; i < line.size();) {
		if (line.compare(i, 8, "{weapon}") == 0) {
			out += weapon;
			i += 8;
		} else if (line.compare(i, 6, "{near}") == 0) {
			out += near_;
			i += 6;
		} else if (line.compare(i, 7, "{actor}") == 0) {
			out += actor;
			i += 7;
		} else if (line.compare(i, 6, "{form:") == 0) {
			std::string plugin;
			UInt32 local = 0;
			const size_t length = ParseFormToken(line, i, plugin, local);
			UInt32 base = 0;
			bool light = false;
			if (length > 0 && resolve != nullptr && *resolve && (*resolve)(plugin, base, light)) {
				char form[16];
				std::snprintf(form, sizeof(form), "%08X", FormIdOfPlugin(base, light, local));
				out += form;
				i += length;
			} else {
				if (unresolved != nullptr) {
					*unresolved = true;
				}
				out += line[i];
				++i;
			}
		} else {
			out += line[i];
			++i;
		}
	}
	return out;
}

// The lines waiting to be run, handed to the game one a frame.
struct ScriptConsoleFeed {
	std::vector<std::string> lines;  // as the script has them
	size_t next = 0;
	UInt32 holdFrames = 0;     // frames to let pass before the next hand-over
	float holdSeconds = 0.0f;  // and game time
	UInt32 refusals = 0;       // the current line refused this many frames in a row
};

// The script's `console` lines, queued once it has started; they wait
// kScriptConsoleStartSettleSeconds.
inline void QueueStartConsoleLines(const HandScript& script, ScriptConsoleFeed& feed) {
	for (const std::string& line : script.console) {
		feed.lines.push_back(line);
	}
	if (!script.console.empty()) {
		feed.holdSeconds = kScriptConsoleStartSettleSeconds;
	}
}

// The script's `console-at <mark> <command>` lines for this mark, queued in
// the script's order; returns how many. They wait two frames: {near} and
// {actor} come from the mark's "HandScript: items" and "HandScript: actors"
// lines, which the camera hook writes later in the mark's own frame.
inline UInt32 QueueConsoleLinesAt(const HandScript& script, const std::string& mark, ScriptConsoleFeed& feed) {
	UInt32 queued = 0;
	for (const std::string& entry : script.consoleAt) {
		const size_t space = entry.find(' ');
		if (space == std::string::npos || entry.compare(0, space, mark) != 0 || space != mark.size()) {
			continue;
		}
		size_t start = space;
		while (start < entry.size() && entry[start] == ' ') {
			++start;
		}
		feed.lines.push_back(entry.substr(start));
		++queued;
	}
	if (queued > 0) {
		feed.holdFrames = 2;
	}
	return queued;
}

// What one frame did with the feed.
struct ScriptConsoleStep {
	bool handed = false;   // a line went to the game
	bool dropped = false;  // a line was given up: too long, or refused too often
	std::string line;      // that line, expanded
};

// One frame of `dt` seconds: nothing while held or with nothing left; else
// the next line, expanded, offered to `run` (true when the game's console
// took it). A line longer than the console takes is given up without being
// offered; a refused one is offered again the next frame, up to
// kScriptConsoleRefusalsMost frames, then given up.
template <typename RunFn>
ScriptConsoleStep StepScriptConsoleFeed(ScriptConsoleFeed& feed, float dt, UInt32 weaponForm, UInt32 nearForm,
                                        UInt32 actorForm, RunFn run, const ScriptFormResolver* resolve = nullptr,
                                        bool* unresolved = nullptr) {
	ScriptConsoleStep step;
	if (feed.holdSeconds > 0.0f) {
		feed.holdSeconds -= dt;
		if (feed.holdSeconds > 0.0f) {
			return step;
		}
	}
	if (feed.holdFrames > 0) {
		--feed.holdFrames;
		return step;
	}
	if (feed.next >= feed.lines.size()) {
		return step;
	}
	step.line = ExpandScriptConsoleLine(feed.lines[feed.next], weaponForm, nearForm, actorForm, resolve, unresolved);
	if (step.line.size() > kScriptConsoleLineMost) {
		step.dropped = true;
		++feed.next;
		feed.refusals = 0;
		return step;
	}
	if (run(step.line)) {
		step.handed = true;
		++feed.next;
		feed.refusals = 0;
		return step;
	}
	if (++feed.refusals >= kScriptConsoleRefusalsMost) {
		step.dropped = true;
		++feed.next;
		feed.refusals = 0;
	}
	return step;
}

// Loading the run's save ([Debug] HandScriptLoad): asked for once the main
// menu has stood kScriptLoadAfterSeconds, asked again while it still stands
// kScriptLoadRetrySeconds after each ask (the console interface not ready yet
// at the first), at most kScriptLoadAsksMost times. Leaving the main menu -
// the load under way - starts its clock over.
inline constexpr float kScriptLoadAfterSeconds = 2.0f;
inline constexpr float kScriptLoadRetrySeconds = 10.0f;
inline constexpr UInt32 kScriptLoadAsksMost = 3;

struct ScriptSaveLoad {
	float mainMenuSeconds = 0.0f;
	UInt32 asked = 0;
};

// The script's `console-menu` lines go to the main menu's console before the
// load, up to kScriptMenuLinesPerFrame a frame (the console's queue takes four
// a frame), once the menu has stood kScriptLoadAfterSeconds; the load waits
// until all of them are handed. For what has to stand before the player's
// model is built - a race's scale (SetRaceScale) for one: the scale sits in
// the player node's matrix from the model's build on, so a line after the
// load changes nothing seen.
inline constexpr UInt32 kScriptMenuLinesPerFrame = 4;

// How many of the menu lines to hand this frame.
inline UInt32 ScriptMenuLinesDue(const ScriptSaveLoad& s, bool haveSave, bool mainMenuUp, size_t handed,
                                 size_t total) {
	if (!haveSave || !mainMenuUp || handed >= total || s.mainMenuSeconds < kScriptLoadAfterSeconds) {
		return 0;
	}
	const size_t left = total - handed;
	return left < kScriptMenuLinesPerFrame ? static_cast<UInt32>(left) : kScriptMenuLinesPerFrame;
}

// Whether to ask for the load in this frame. `menuLinesLeft`: the script's
// console-menu lines not all handed yet - the clock runs, the ask waits.
inline bool StepScriptSaveLoad(ScriptSaveLoad& s, bool haveSave, bool mainMenuUp, float dt,
                               bool menuLinesLeft = false) {
	if (!haveSave) {
		return false;
	}
	if (!mainMenuUp) {
		s.mainMenuSeconds = 0.0f;
		return false;
	}
	s.mainMenuSeconds += dt;
	if (s.asked >= kScriptLoadAsksMost || menuLinesLeft) {
		return false;
	}
	const float due = kScriptLoadAfterSeconds + kScriptLoadRetrySeconds * static_cast<float>(s.asked);
	if (s.mainMenuSeconds < due) {
		return false;
	}
	++s.asked;
	return true;
}

}  // namespace obvr::test
