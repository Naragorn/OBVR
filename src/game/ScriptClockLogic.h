#pragma once

// The script clock ([Debug] ScriptTimeTrace): how long the game's scripts
// take, script by script, frame by frame - the instrument for the long
// frames of 2026-10-25 (docs/performance-spec.md section 6), whose time the
// render sampler found inside xOBSE's script token reader without saying
// whose script it was.
//
// Read in Oblivion.exe 1.2.0.416 (2026-10-25):
//
// - Every script run goes through 0x00517330, a thiscall on a ScriptRunner
//   (0xA4 bytes) with eight arguments and `ret 20h`: the Script, the
//   reference it runs on, its event list, the containing reference, then a
//   byte and two bytes and a float that only an effect script uses. Its two
//   callers are 0x005178FA and 0x0051793C in 0x00517810, which takes the
//   cached runner at 0x00B36208 or, while that one is busy, a new one - so a
//   script that makes another script run (an activation, a nested Execute)
//   runs inside the first one's run. Script::Execute (0x004FBE00, xOBSE's
//   kScript_ExecuteFnAddr) and the three calls at 0x004F9F20..0x004F9F82
//   all reach it that way.
// - Inside it, 0x005169A0 runs one line (one command) at a time: the
//   function xOBSE patches at 0x005169AE to keep the running Script for its
//   error messages. The xOBSE token reader of 2026-10-25 runs under it.
// - The Script (xOBSE Script.h): the form ID at +0x0C (TESForm), the type
//   at +0x28 (ScriptInfo::type: 0 an object's script, 1 a quest's, 0x100 a
//   magic effect's), the compiled data at +0x30.
// - The engine has a script timer of its own: when 0x005792E0 says so (a
//   byte at +0x50 of the object 0x00582160 returns, unidentified), the run
//   reads QueryPerformanceCounter before and after and adds the seconds to
//   the float at Script+0x34. Nothing in OBVR turns it on: it would give a
//   sum per script and nothing per frame or reference.
//
// OBVR detours the run's entry (game/ScriptClock.cpp) and hands each run's
// entry and exit to the clock here, which keeps the open runs on a stack -
// a run's own time is its time minus the runs nested inside it - and sums
// the closed runs per script, for the frame and for a ten-second window. At
// the end of a frame it says whether a line is due: a frame line when the
// frame's scripts took the threshold or longer (at most thirty in a
// window), a window line every ten seconds. The pure half; no Windows here.

#include <cstdarg>
#include <cstdio>

#include "core/Types.h"

namespace obvr::game {

// What kind of script ran, from the Script's type at +0x28.
enum class ScriptKind : UInt8 { Object, Quest, Effect, Other };

inline ScriptKind ScriptKindOf(UInt32 type) {
	switch (type) {
	case 0: return ScriptKind::Object;
	case 1: return ScriptKind::Quest;
	case 0x100: return ScriptKind::Effect;
	default: return ScriptKind::Other;
	}
}

inline const char* ScriptKindName(ScriptKind kind) {
	switch (kind) {
	case ScriptKind::Object: return "object";
	case ScriptKind::Quest: return "quest";
	case ScriptKind::Effect: return "effect";
	default: return "other";
	}
}

// [Debug] ScriptTimeTraceMs: a frame whose scripts take this long gets a
// line. Held to 0.1 .. 1000 ms; not a number is the default.
inline constexpr float kScriptClockDefaultThresholdMs = 4.0f;
inline constexpr float kScriptClockMinThresholdMs = 0.1f;
inline constexpr float kScriptClockMaxThresholdMs = 1000.0f;

inline float ScriptClockThresholdMs(float value) {
	if (!(value == value)) return kScriptClockDefaultThresholdMs;
	if (value < kScriptClockMinThresholdMs) return kScriptClockMinThresholdMs;
	if (value > kScriptClockMaxThresholdMs) return kScriptClockMaxThresholdMs;
	return value;
}

// One closed run.
struct ScriptRun {
	UInt32 scriptId = 0;
	UInt32 refId = 0;            // the reference it ran on, 0 for none
	ScriptKind kind = ScriptKind::Other;
	long long inclusiveTicks = 0;  // entry to exit, the runs nested in it included
	long long selfTicks = 0;       // without them
};

// The runs open right now, innermost last. Runs nested deeper than the
// stack holds are counted but not timed: their time stays the deepest held
// run's own.
class ScriptRunStack {
public:
	static constexpr UInt32 kMaxDepth = 32;

	void Enter(UInt32 scriptId, UInt32 refId, ScriptKind kind, long long now) {
		if (depth_ < kMaxDepth) {
			Open& open = open_[depth_];
			open.scriptId = scriptId;
			open.refId = refId;
			open.kind = kind;
			open.start = now;
			open.nested = 0;
		}
		++depth_;
	}

	// Closes the innermost open run into `run`. False when nothing is open,
	// or when the run closed was one of the untimed ones beyond the stack.
	bool Exit(long long now, ScriptRun& run) {
		if (depth_ == 0) return false;
		--depth_;
		if (depth_ >= kMaxDepth) return false;
		const Open& open = open_[depth_];
		long long inclusive = now - open.start;
		if (inclusive < 0) inclusive = 0;
		long long self = inclusive - open.nested;
		if (self < 0) self = 0;
		run.scriptId = open.scriptId;
		run.refId = open.refId;
		run.kind = open.kind;
		run.inclusiveTicks = inclusive;
		run.selfTicks = self;
		if (depth_ > 0) open_[depth_ - 1].nested += inclusive;
		return true;
	}

	UInt32 Depth() const { return depth_; }
	void Reset() { depth_ = 0; }

private:
	struct Open {
		UInt32 scriptId = 0;
		UInt32 refId = 0;
		ScriptKind kind = ScriptKind::Other;
		long long start = 0;
		long long nested = 0;
	};
	Open open_[kMaxDepth] = {};
	UInt32 depth_ = 0;
};

// One script's share of a frame or a window.
struct ScriptTimeEntry {
	UInt32 scriptId = 0;
	ScriptKind kind = ScriptKind::Other;
	bool used = false;
	UInt32 runs = 0;
	long long selfTicks = 0;
	long long longestTicks = 0;   // the longest single run, nested runs included
	UInt32 longestRefId = 0;      // the reference that run ran on
};

// The closed runs summed per script, in a fixed open-addressed table: no
// allocation inside a script run. Past kMaxScripts distinct scripts the
// rest are summed as "other", so the probes stay short. Loading a save with
// Maskar's Oblivion Overhaul ran more than 384 distinct scripts in one
// frame and ten seconds (the harness run of 2026-10-25 17:36, 36213 runs
// beyond a table of that size), hence room for 3072.
class ScriptTimeTable {
public:
	static constexpr UInt32 kSlots = 4096;   // a power of two
	static constexpr UInt32 kMaxScripts = 3072;

	void Add(const ScriptRun& run) {
		++runs_;
		ticks_ += run.selfTicks;
		ScriptTimeEntry* entry = Find(run.scriptId, run.kind);
		if (entry == nullptr) {
			++otherRuns_;
			otherTicks_ += run.selfTicks;
			return;
		}
		++entry->runs;
		entry->selfTicks += run.selfTicks;
		if (entry->runs == 1 || run.inclusiveTicks > entry->longestTicks) {
			entry->longestTicks = run.inclusiveTicks;
			entry->longestRefId = run.refId;
		}
	}

	void Clear() {
		for (UInt32 i = 0; i < used_; ++i) slots_[usedSlots_[i]] = ScriptTimeEntry{};
		used_ = 0;
		runs_ = 0;
		ticks_ = 0;
		otherRuns_ = 0;
		otherTicks_ = 0;
	}

	UInt32 Scripts() const { return used_; }
	UInt32 Runs() const { return runs_; }
	long long SelfTicks() const { return ticks_; }
	UInt32 OtherRuns() const { return otherRuns_; }
	long long OtherTicks() const { return otherTicks_; }

	// The `capacity` scripts with the most own time, the most first; equal
	// times by form ID. Returns how many were written.
	UInt32 Top(const ScriptTimeEntry** out, UInt32 capacity) const {
		UInt32 count = 0;
		for (UInt32 i = 0; i < used_; ++i) {
			const ScriptTimeEntry* entry = &slots_[usedSlots_[i]];
			UInt32 at = count;
			while (at > 0 && Before(*entry, *out[at - 1])) --at;
			if (at >= capacity) continue;
			const UInt32 last = count < capacity ? count : capacity - 1;
			for (UInt32 j = last; j > at; --j) out[j] = out[j - 1];
			out[at] = entry;
			if (count < capacity) ++count;
		}
		return count;
	}

private:
	static bool Before(const ScriptTimeEntry& a, const ScriptTimeEntry& b) {
		if (a.selfTicks != b.selfTicks) return a.selfTicks > b.selfTicks;
		return a.scriptId < b.scriptId;
	}

	// The entry for scriptId, made when there is room; null when the table
	// is full and the script is not in it.
	ScriptTimeEntry* Find(UInt32 scriptId, ScriptKind kind) {
		UInt32 slot = (scriptId ^ (scriptId >> 16)) & (kSlots - 1);
		for (UInt32 probe = 0; probe < kSlots; ++probe) {
			ScriptTimeEntry& entry = slots_[slot];
			if (!entry.used) {
				if (used_ >= kMaxScripts) return nullptr;
				entry.used = true;
				entry.scriptId = scriptId;
				entry.kind = kind;
				usedSlots_[used_++] = static_cast<UInt16>(slot);
				return &entry;
			}
			if (entry.scriptId == scriptId) return &entry;
			slot = (slot + 1) & (kSlots - 1);
		}
		return nullptr;
	}

	ScriptTimeEntry slots_[kSlots] = {};
	UInt16 usedSlots_[kMaxScripts] = {};
	UInt32 used_ = 0;
	UInt32 runs_ = 0;
	long long ticks_ = 0;
	UInt32 otherRuns_ = 0;
	long long otherTicks_ = 0;
};

// Which lines FrameEnd wrote.
struct ScriptClockLines {
	bool frameLine = false;
	bool windowLine = false;
};

class ScriptClock {
public:
	static constexpr UInt32 kTop = 5;                    // scripts a line names
	static constexpr UInt32 kFrameLinesPerWindow = 30;
	static constexpr long long kWindowSeconds = 10;

	void Enter(UInt32 scriptId, UInt32 refId, ScriptKind kind, long long now) {
		stack_.Enter(scriptId, refId, kind, now);
	}

	void Exit(long long now) {
		ScriptRun run;
		if (stack_.Exit(now, run)) {
			frame_.Add(run);
			window_.Add(run);
		}
	}

	// The end of a frame at `now`. untimedRuns: the runs since the last call
	// that were not timed because they ran off the game's thread. Writes the
	// frame line when the frame's scripts took thresholdMs or longer and the
	// window's budget has room, the window line when ten seconds have passed
	// since the window opened; a line that does not fit its buffer is not
	// written at all. The first call opens the window. A frequency of zero
	// writes nothing.
	ScriptClockLines FrameEnd(long long now, long long frequency, double thresholdMs, UInt32 untimedRuns,
	                          char* frameLine, UInt32 frameCapacity, char* windowLine, UInt32 windowCapacity) {
		ScriptClockLines lines;
		if (!windowOpen_) {
			windowOpen_ = true;
			windowStart_ = now;
		}
		++windowFrames_;
		windowUntimed_ += untimedRuns;
		const long long frameTicks = frame_.SelfTicks();
		if (frameTicks > windowLongest_) windowLongest_ = frameTicks;
		if (frequency > 0) {
			const long long threshold = static_cast<long long>(thresholdMs * static_cast<double>(frequency) / 1000.0);
			if (thresholdMs > 0.0 && frameTicks > 0 && frameTicks >= threshold) {
				++windowOver_;
				if (windowLines_ < kFrameLinesPerWindow) {
					lines.frameLine = WriteFrameLine(now, frequency, frameLine, frameCapacity);
					if (lines.frameLine) ++windowLines_;
				} else {
					++windowHeld_;
				}
			}
		}
		frame_.Clear();
		if (frequency > 0 && now - windowStart_ >= kWindowSeconds * frequency) {
			lines.windowLine = WriteWindowLine(now, frequency, thresholdMs, windowLine, windowCapacity);
			OpenWindow(now);
		}
		return lines;
	}

	// Forgets everything: the open runs, the frame, the window.
	void Reset() {
		stack_.Reset();
		frame_.Clear();
		OpenWindow(0);
		windowOpen_ = false;
	}

private:
	void OpenWindow(long long now) {
		window_.Clear();
		windowStart_ = now;
		windowFrames_ = 0;
		windowUntimed_ = 0;
		windowLongest_ = 0;
		windowOver_ = 0;
		windowLines_ = 0;
		windowHeld_ = 0;
	}

	static double Ms(long long ticks, long long frequency) {
		return static_cast<double>(ticks) * 1000.0 / static_cast<double>(frequency);
	}

	// Appends to out at used; false when it does not fit.
#if defined(__clang__) || defined(__GNUC__)
	__attribute__((format(printf, 4, 5)))
#endif
	static bool Append(char* out, UInt32 capacity, UInt32& used, const char* format, ...) {
		if (used >= capacity) return false;
		std::va_list args;
		va_start(args, format);
		const int n = std::vsnprintf(out + used, capacity - used, format, args);
		va_end(args);
		if (n < 0 || used + static_cast<UInt32>(n) >= capacity) return false;
		used += static_cast<UInt32>(n);
		return true;
	}

	// "1F012345 quest 60.0 ms in 1000 runs, the longest 0.06 ms on 00000014",
	// the top scripts of the table, then what the line leaves out.
	static bool AppendScripts(const ScriptTimeTable& table, long long frequency, char* out, UInt32 capacity,
	                          UInt32& used) {
		const ScriptTimeEntry* top[kTop] = {};
		const UInt32 count = table.Top(top, kTop);
		long long shown = 0;
		for (UInt32 i = 0; i < count; ++i) {
			const ScriptTimeEntry& e = *top[i];
			shown += e.selfTicks;
			if (!Append(out, capacity, used, "%s%08X %s %.1f ms in %u run%s, the longest %.2f ms", i == 0 ? "" : "; ",
			            e.scriptId, ScriptKindName(e.kind), Ms(e.selfTicks, frequency), e.runs,
			            e.runs == 1 ? "" : "s", Ms(e.longestTicks, frequency))) {
				return false;
			}
			const bool ok = e.longestRefId != 0 ? Append(out, capacity, used, " on %08X", e.longestRefId)
			                                     : Append(out, capacity, used, " without a reference");
			if (!ok) return false;
		}
		if (table.Scripts() > count) {
			const long long rest = table.SelfTicks() - table.OtherTicks() - shown;
			if (!Append(out, capacity, used, "; and %u more scripts %.1f ms", table.Scripts() - count,
			            Ms(rest, frequency))) {
				return false;
			}
		}
		if (table.OtherRuns() > 0 &&
		    !Append(out, capacity, used, "; %u runs of scripts beyond the table's %u %.1f ms", table.OtherRuns(),
		            ScriptTimeTable::kMaxScripts, Ms(table.OtherTicks(), frequency))) {
			return false;
		}
		return true;
	}

	bool WriteFrameLine(long long now, long long frequency, char* out, UInt32 capacity) const {
		if (out == nullptr || capacity == 0) return false;
		UInt32 used = 0;
		const bool ok =
			Append(out, capacity, used, "a frame's scripts took %.1f ms in %u run%s of %u script%s, at tick %lld: ",
			       Ms(frame_.SelfTicks(), frequency), frame_.Runs(), frame_.Runs() == 1 ? "" : "s", frame_.Scripts(),
			       frame_.Scripts() == 1 ? "" : "s", now) &&
			AppendScripts(frame_, frequency, out, capacity, used);
		if (!ok) out[0] = 0;
		return ok;
	}

	bool WriteWindowLine(long long now, long long frequency, double thresholdMs, char* out, UInt32 capacity) const {
		if (out == nullptr || capacity == 0) return false;
		UInt32 used = 0;
		const double average =
			windowFrames_ > 0 ? Ms(window_.SelfTicks(), frequency) / static_cast<double>(windowFrames_) : 0.0;
		bool ok = Append(out, capacity, used,
		                 "%.1f s, %u frames: scripts %.2f ms a frame on average, the longest frame's %.1f ms; "
		                 "%u frames at %.1f ms or more (%u of their lines held back)",
		                 Ms(now - windowStart_, frequency) / 1000.0, windowFrames_, average,
		                 Ms(windowLongest_, frequency), windowOver_, thresholdMs, windowHeld_);
		if (ok && windowUntimed_ > 0) {
			ok = Append(out, capacity, used, "; %u runs off the game's thread not timed", windowUntimed_);
		}
		if (ok) {
			ok = window_.Runs() == 0 ? Append(out, capacity, used, "; no script ran")
			                         : Append(out, capacity, used, "; the most time: ") &&
			                               AppendScripts(window_, frequency, out, capacity, used);
		}
		if (!ok) out[0] = 0;
		return ok;
	}

	ScriptRunStack stack_;
	ScriptTimeTable frame_;
	ScriptTimeTable window_;
	bool windowOpen_ = false;
	long long windowStart_ = 0;
	UInt32 windowFrames_ = 0;
	UInt32 windowUntimed_ = 0;
	long long windowLongest_ = 0;
	UInt32 windowOver_ = 0;
	UInt32 windowLines_ = 0;
	UInt32 windowHeld_ = 0;
};

}  // namespace obvr::game
