#pragma once

#include "core/Types.h"

namespace obvr::config {

// The decision half of the hot reload, kept apart from the file system so
// every flow can be exercised without one.
//
// Why it exists: Config::Reload re-read the whole INI every ReloadEveryFrames
// frames whether or not anything had changed - 374 GetPrivateProfileString
// calls over a 119 KB file, measured at about 75 ms on the 7800X3D, once every
// 1.3 seconds at 90 Hz, on the thread that renders the frame. Seven dropped
// frames a second and a half, which the headset showed as a regular stutter.
// Asking the file whether it changed costs one directory read.

// What the file system says about the two files the reload reads: OBVR.ini
// and, beside it, OBVR-test.ini, which only the test runners write. Zero for
// a file that is not there.
struct FileStamp {
	UInt64 iniWrite = 0;
	UInt64 iniSize = 0;
	UInt64 overlayWrite = 0;
	UInt64 overlaySize = 0;
	// false when OBVR.ini could not be asked - missing, or locked for the
	// instant of the question. The overlay has no such flag: its absence is a
	// state of its own, the one every ordinary run is in.
	bool iniKnown = false;
};

inline bool operator==(const FileStamp& a, const FileStamp& b) {
	return a.iniKnown == b.iniKnown && a.iniWrite == b.iniWrite && a.iniSize == b.iniSize &&
	       a.overlayWrite == b.overlayWrite && a.overlaySize == b.overlaySize;
}

inline bool operator!=(const FileStamp& a, const FileStamp& b) { return !(a == b); }

struct ReloadGate {
	FileStamp seen;
};

enum class ReloadVerdict {
	Unreadable,  // OBVR.ini could not be asked this time: keep what is loaded
	Unchanged,   // the same files as last time: nothing to read
	Changed      // something differs: read, and remember this state
};

// One check of the gate against the files as they are now. Changed is also the
// answer when nothing was ever remembered - a Load that found no file still
// has to notice one appearing later.
inline ReloadVerdict Decide(ReloadGate& gate, const FileStamp& now) {
	if (!now.iniKnown) {
		return ReloadVerdict::Unreadable;
	}
	if (gate.seen.iniKnown && gate.seen == now) {
		return ReloadVerdict::Unchanged;
	}
	gate.seen = now;
	return ReloadVerdict::Changed;
}

// What Load remembers once it has read the files, so the first reload after
// it answers Unchanged rather than reading everything a second time.
inline void Remember(ReloadGate& gate, const FileStamp& now) {
	gate.seen = now;
}

}  // namespace obvr::config
