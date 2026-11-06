#include "game/KnockWatch.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/MathFns.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "game/GameTypes.h"
#include "game/MeleeHits.h"
#include "game/RefFingerprint.h"

namespace obvr::game {

namespace {

constexpr float kMarks[] = {0.0f, 0.5f, 1.0f, 2.0f, 4.0f};
constexpr UInt32 kMarkCount = sizeof(kMarks) / sizeof(kMarks[0]);

struct Watch {
	UInt32 actor = 0;
	RefFingerprint print{};
	float force = 0.0f;
	float sourceOver = 0.0f;
	float bound = 0.0f;  // their bound's radius, units
	bool byRoot = false;  // no pelvis and head by their names: the root node followed
	float seconds = 0.0f;
	UInt32 next = 0;  // the next mark to log
	bool haveStart = false;
	NiPoint3 pelvis{0.0f, 0.0f, 0.0f};
	NiPoint3 head{0.0f, 0.0f, 0.0f};
};

Watch& TheWatch() {
	static Watch w;
	return w;
}

using GetObjectFn = void*(__fastcall*)(void* self, void* edx, const char* name);

bool BonePosition(UInt32 actor, const char* name, NiPoint3& out) {
	const UInt32 root = *reinterpret_cast<const UInt32*>(actor + addr::kRefNiNodeOffset);
	const UInt32 vtable = mem::LooksLikeObjectAddress(root) ? *reinterpret_cast<const UInt32*>(root) : 0;
	const UInt32 fn = mem::LooksLikeObjectAddress(vtable)
		                  ? *reinterpret_cast<const UInt32*>(vtable + addr::kNiAVObjectGetObjectVtableOffset)
		                  : 0;
	if (fn < addr::kTextStart || fn >= addr::kTextEnd) {
		return false;
	}
	const auto* const node = static_cast<const NiAVObject*>(
		reinterpret_cast<GetObjectFn>(fn)(reinterpret_cast<void*>(root), nullptr, name));
	if (!mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(node))) {
		return false;
	}
	out = node->worldTransform.pos;
	return true;
}

// The reference's own root node's place - or, with no node (a body whose 3D
// went), the place the reference itself holds.
bool RootPosition(UInt32 actor, NiPoint3& out) {
	const UInt32 root = *reinterpret_cast<const UInt32*>(actor + addr::kRefNiNodeOffset);
	if (!mem::LooksLikeObjectAddress(root)) {
		out = *reinterpret_cast<const NiPoint3*>(actor + addr::kRefPositionOffset);
		return out.x == out.x && out.y == out.y && out.z == out.z;
	}
	out = reinterpret_cast<const NiAVObject*>(root)->worldTransform.pos;
	return true;
}

float Flat(const NiPoint3& d) { return math::Sqrt(d.x * d.x + d.y * d.y); }

}  // namespace

void WatchKnockdown(UInt32 actor, float force, float sourceOverMiddle) {
	Watch& w = TheWatch();
	w = Watch{};
	w.actor = actor;
	w.print = TakeRefFingerprint(actor);
	w.force = force;
	w.sourceOver = sourceOverMiddle;
	w.bound = ActorBoundRadius(reinterpret_cast<void*>(actor));
}

void StepKnockWatch(float dtSeconds, bool enabled) {
	Watch& w = TheWatch();
	if (w.actor == 0) {
		return;
	}
	if (!enabled || !RefStillThere(w.print)) {
		if (enabled) {
			OBVR_LOG("Knock watch: %08X is gone %.1f s after the knockdown - no longer followed", w.actor,
			         static_cast<double>(w.seconds));
		}
		w = Watch{};
		return;
	}
	NiPoint3 pelvis{};
	NiPoint3 head{};
	if (!BonePosition(w.actor, "Bip01 Pelvis", pelvis) || !BonePosition(w.actor, "Bip01 Head", head)) {
		// A creature of another skeleton: its root node.
		if (!RootPosition(w.actor, pelvis)) {
			OBVR_LOG("Knock watch: %08X has no place to read %.1f s after the knockdown - no longer followed", w.actor,
			         static_cast<double>(w.seconds));
			w = Watch{};
			return;
		}
		head = pelvis;
		w.byRoot = true;
	}
	if (!w.haveStart) {
		w.pelvis = pelvis;
		w.head = head;
		w.haveStart = true;
	} else if (dtSeconds > 0.0f && dtSeconds < 1.0f) {
		w.seconds += dtSeconds;
	}
	if (w.next < kMarkCount && w.seconds >= kMarks[w.next]) {
		const NiPoint3 moved = pelvis - w.pelvis;
		if (w.byRoot) {
			OBVR_LOG("Knock watch: %08X %.1f s after the knockdown (force %.1f, its source %.1f units over their "
			         "middle, their bound %.0f units round) - its root %.1f units away flat, %.1f up (no pelvis and "
			         "head by their names on it)",
			         w.actor, static_cast<double>(w.seconds), static_cast<double>(w.force),
			         static_cast<double>(w.sourceOver), static_cast<double>(w.bound), static_cast<double>(Flat(moved)),
			         static_cast<double>(moved.z));
		} else {
			OBVR_LOG("Knock watch: %08X %.1f s after the knockdown (force %.1f, its source %.1f units over their "
			         "middle, their bound %.0f units round) - the pelvis %.1f units away flat, %.1f up; the head %.1f "
			         "units over the pelvis (stood %.1f), %.1f flat from it",
			         w.actor, static_cast<double>(w.seconds), static_cast<double>(w.force),
			         static_cast<double>(w.sourceOver), static_cast<double>(w.bound), static_cast<double>(Flat(moved)),
			         static_cast<double>(moved.z),
			         static_cast<double>(head.z - pelvis.z), static_cast<double>(w.head.z - w.pelvis.z),
			         static_cast<double>(Flat(head - pelvis)));
		}
		++w.next;
	}
	if (w.next >= kMarkCount) {
		w = Watch{};
	}
}

}  // namespace obvr::game
