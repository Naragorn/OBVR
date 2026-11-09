#include "game/MeleeSweep.h"

#include "core/Log.h"
#include "game/RefFingerprint.h"

namespace obvr::game {
namespace {

// Every swing, no budget: the next headset run says whether the frames'
// jumps were the misses (spec 26, C).
void LogSummary(const SwingSweepSummary& s) {
	OBVR_LOG("Hands: swing %u (%s) - %u frames, %u swept from the frame before, the tip's largest step %.0f units, "
	         "%.0f fps (longest frame %.1f ms); bodies touched at a frame %u, only between two frames %u, struck %u",
	         s.serial, s.hand == 0 ? "weapon hand" : "other hand", s.frames, s.sweptFrames,
	         static_cast<double>(s.mostJump), static_cast<double>(SummaryFramesPerSecond(s)),
	         static_cast<double>(s.longestFrame * 1000.0f), s.touchedNow, s.touchedSwept, s.struck);
	if (s.struck == 0) {
		const StrikeWalkTally& w = s.walk;
		OBVR_LOG("Hands: swing %u struck nothing - weapon type %d, reach %.0f; list entries %u, actors %u, alive %u, "
		         "bounded %u (first other table %08X); %u bodies near, the nearest %.0f units from the blade, %.0f "
		         "needed",
		         s.serial, s.weaponType, static_cast<double>(s.reach), w.entries, w.actors, w.alive, w.bounded,
		         w.otherVtable, w.bodies, static_cast<double>(w.nearest), static_cast<double>(w.needed));
	}
}

}  // namespace

StrikeSweep BeginStrikeSweep(StrikeSweeps* sweeps, UInt32 slot, UInt32 serial, bool swinging, UInt32 frame,
                             float frameSeconds, const NiPoint3& base, const NiPoint3& tip) {
	StrikeSweep sweep;
	if (sweeps == nullptr || slot >= kStrikeSweepSlots) {
		return sweep;
	}
	SwingSweepSummary& summary = sweeps->summary[slot];
	if (swinging && summary.serial != serial) {
		if (summary.frames > 0) {
			LogSummary(summary);
		}
		StartSummary(summary, serial, slot);
	}
	BladeSweepMemory& seen = sweeps->seen[slot];
	sweep.from = SweepStart(seen, frame, CurrentWorld(), base, tip);
	const bool swept = sweep.from == SweepFrom::Swept;
	if (swept) {
		sweep.steps = SweepSteps(seen, base, tip);
		sweep.fromBase = seen.base;
		sweep.fromTip = seen.tip;
	}
	if (swinging && sweep.from != SweepFrom::SameFrame) {
		NoteSummaryFrame(summary, sweep.from, swept ? TipJumpUnits(seen, tip) : 0.0f, frameSeconds);
	}
	if (sweep.from != SweepFrom::NotANumber) {
		RememberBlade(seen, frame, CurrentWorld(), base, tip);
	}
	return sweep;
}

void NoteStrikeTouch(StrikeSweeps* sweeps, UInt32 slot, bool swept) {
	if (sweeps == nullptr || slot >= kStrikeSweepSlots) {
		return;
	}
	SwingSweepSummary& summary = sweeps->summary[slot];
	if (swept) {
		++summary.touchedSwept;
	} else {
		++summary.touchedNow;
	}
}

void NoteStrikeWalk(StrikeSweeps* sweeps, UInt32 slot, const StrikeWalkTally& frame, SInt32 weaponType, float reach,
                    UInt32 struck) {
	if (sweeps == nullptr || slot >= kStrikeSweepSlots) {
		return;
	}
	SwingSweepSummary& summary = sweeps->summary[slot];
	AddWalkTally(summary.walk, frame);
	summary.weaponType = weaponType;
	summary.reach = reach;
	summary.struck += struck;
}

void ForgetStrikeBlades(StrikeSweeps& sweeps) {
	for (UInt32 i = 0; i < kStrikeSweepSlots; ++i) {
		sweeps.seen[i] = BladeSweepMemory{};
	}
}

}  // namespace obvr::game
