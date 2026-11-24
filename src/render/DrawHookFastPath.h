#pragma once

#include "core/Types.h"
#include "render/PointerSet.h"

// What the device hooks decide on every draw and every constant upload, so
// that a world draw does not pay for what only the menu's pass, the bone lock
// or the water needs. Measured on Windows, 2026-11-23 (the fight in the
// tutorial dungeon, about 2 400 pass records a frame, each drawn for two eyes):
// OBVR's hooks took 0.99 ms of an 11.2 ms scene pass, and the two searches of
// the dynamic pool's set in every indexed draw were its two hottest
// instructions. With these decisions the same fight under Linux went from
// 0.475 to 0.280 ms a frame in OBVR.dll (docs/performance-draw-hooks.md).

namespace obvr::render {

// Whether stream 0 is a buffer of the dynamic pool (PointerSet.h), asked of
// the set once per binding instead of at every draw: the draw hooks read it
// for their skinned-draw counts, twice in an indexed draw.
//
// The answer holds while the same buffer stays bound. A yes holds for good -
// the set only grows. A no goes stale when that buffer is locked with DISCARD
// for the first time while bound, which the lock hook reports (NoteInserted).
struct Stream0Membership {
	const void* buffer = nullptr;
	bool known = false;
	bool dynamic = false;

	// SetStreamSource(0, bound). The pool's buffer is bound again for every
	// skinned shape, at another offset: the same buffer keeps its answer.
	void Bind(const void* bound) {
		if (bound == buffer) {
			return;
		}
		buffer = bound;
		known = false;
		dynamic = false;
	}

	// A buffer was handed to the set (it may have refused it, full).
	void NoteInserted(const void* inserted) {
		if (inserted == buffer && !dynamic) {
			known = false;
		}
	}

	// contains: the set's search, called only when the answer is not known.
	template <typename Contains>
	bool DynamicBy(Contains&& contains) {
		if (buffer == nullptr) {
			return false;
		}
		if (!known) {
			dynamic = contains(buffer);
			known = true;
		}
		return dynamic;
	}

	bool Dynamic(const PointerSet& pool) {
		return DynamicBy([&pool](const void* bound) { return pool.Contains(bound); });
	}
};

// What one hook call learns for the hooks that follow it, so that they need
// not ask again.
struct DrawHookMemo {
	Stream0Membership stream0;
	// The water's replacement vertex shader is the one selected: asked of
	// WaterReprojection at every SetVertexShader, read at every constant
	// upload. It can read yes after the water's code dropped the shader (a
	// reflection sub-pass, a device reset) - that costs the call it would
	// have saved and nothing else; it never reads no while the shader is on.
	bool waterShaderSelected = false;
};

// Whether a draw has anything for the after-pass sampler: inside the menu's
// pass only while its tail ring is armed, outside it only while samples are
// still owed. A world draw has neither, and is not handed to it.
inline bool AfterPassSampleDue(bool inInterfacePass, bool tailArmed, UInt32 samplesOwed) {
	return inInterfacePass ? tailArmed : samplesOwed > 0;
}

// Whether a vertex constant upload is looked at as a bone row at all: only
// while a bone lock runs (the dual pass's capture and replace) or the upload
// stack probe is still owed. The single pass runs neither.
inline bool BoneRowsWatched(bool boneLockRunning, UInt32 stackProbesOwed) {
	return boneLockRunning || stackProbesOwed > 0;
}

}  // namespace obvr::render
