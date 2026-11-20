#include "game/PhysicsPlan.h"

#include "core/AroundCall.h"
#include "core/EntryDetour.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/PhysicsPlanLogic.h"
#include "platform/Win32Min.h"

namespace obvr::game {
namespace {

constexpr UInt32 kPlannerCall = 0x0040D8C2;  // OSGlobals::UpdateStep: call 0x00889810
constexpr UInt32 kPlanner = 0x00889810;      // PlanHavokSteps, cdecl(float frameSeconds, byte flag)
constexpr UInt32 kFrameSeconds = 0x00B2E2E4;
constexpr UInt32 kSimulated = 0x00B2E2E0;
constexpr UInt32 kCarry = 0x00BA7910;
constexpr UInt32 kStepCount = 0x00BA7914;
constexpr UInt32 kStepSeconds = 0x00BA790C;
constexpr UInt32 kUpdateType = 0x00BA7918;
constexpr UInt32 kMaxStep = 0x00B2E2E8;
constexpr UInt32 kWorldStep = 0x00898B70;  // hkWorld::stepDeltaTime, thiscall(world, float), ret 4
constexpr UInt8 kWorldStepEntry[5] = {0x83, 0xEC, 0x08, 0x56, 0x57};  // sub esp,8; push esi; push edi

template <typename T>
T& At(UInt32 address) {
	return *reinterpret_cast<T*>(address);
}

struct PlanState {
	bool wanted = false;
	bool tried = false;
	bool installed = false;
	bool usedSaid = false;
};

PlanState& Plan() {
	static PlanState s_plan;
	return s_plan;
}

using PlannerFn = void(__cdecl*)(float, UInt32);

// In place of the engine's planner, called where it was (0x0040D8C2).
void __cdecl PlanSteps(float frameSeconds, UInt32 flag) {
	PlanState& s = Plan();
	if (!s.wanted || At<SInt32>(kUpdateType) != 0) {
		reinterpret_cast<PlannerFn>(kPlanner)(frameSeconds, flag);
		return;
	}
	const PhysicsStepPlan p =
		PlanEvenPhysicsSteps(frameSeconds, At<float>(kCarry), At<float>(kMaxStep), (flag & 0xFF) != 0 ? 2u : 3u);
	At<float>(kFrameSeconds) = frameSeconds;
	At<float>(kSimulated) = p.simulated;
	At<float>(kCarry) = p.carry;
	At<UInt32>(kStepCount) = p.steps;
	At<float>(kStepSeconds) = p.stepSeconds;
	if (!s.usedSaid) {
		s.usedSaid = true;
		OBVR_LOG("Physics: the steps planned evenly - this frame %.2f ms in %u step(s) of %.2f ms, the maximum %.2f ms",
		         static_cast<double>(frameSeconds * 1000.0f), p.steps, static_cast<double>(p.stepSeconds * 1000.0f),
		         static_cast<double>(At<float>(kMaxStep) * 1000.0f));
	}
}

bool InstallPlanner() {
	const UInt32 d = mem::CallRelativeDisplacement(kPlannerCall, kPlanner);
	const UInt8 expected[5] = {0xE8, static_cast<UInt8>(d & 0xFF), static_cast<UInt8>((d >> 8) & 0xFF),
	                           static_cast<UInt8>((d >> 16) & 0xFF), static_cast<UInt8>((d >> 24) & 0xFF)};
	if (!mem::Verify(kPlannerCall, expected, sizeof(expected))) {
		OBVR_LOG("Physics: the call at %08X is not `call %08X` - the engine plans its steps itself", kPlannerCall,
		         kPlanner);
		mem::ReportForeignCode("Physics", kPlannerCall);
		return false;
	}
	UInt8 patch[5];
	if (mem::BuildCallSitePatch(patch, sizeof(patch), kPlannerCall, reinterpret_cast<UInt32>(&PlanSteps)) !=
	        sizeof(patch) ||
	    !mem::SafeWrite(kPlannerCall, patch, sizeof(patch))) {
		OBVR_LOG("Physics: could not reroute the planner's call at %08X", kPlannerCall);
		return false;
	}
	return true;
}

// ---------------------------------------------------------------- step times

struct StepTimes {
	bool tried = false;
	UInt32 trampoline = 0;
	UInt32 steps = 0;
	long long ticks = 0;
	long long longest = 0;
	UInt32 frames = 0;  // frames since the last mark (StepPhysicsStepTimes calls)
};

StepTimes& Times() {
	static StepTimes s_times;
	return s_times;
}

using WorldStepFn = int(__thiscall*)(void*, float);

int __fastcall TimedWorldStep(void* world, void* /*edx*/, float seconds) {
	StepTimes& t = Times();
	const long long before = ReadPerformanceCounter();
	const int result = reinterpret_cast<WorldStepFn>(t.trampoline)(world, seconds);
	const long long spent = ReadPerformanceCounter() - before;
	++t.steps;
	t.ticks += spent;
	if (spent > t.longest) {
		t.longest = spent;
	}
	return result;
}

bool InstallStepTimes() {
	StepTimes& t = Times();
	if (!mem::Verify(kWorldStep, kWorldStepEntry, sizeof(kWorldStepEntry))) {
		OBVR_LOG("Physics: step times - the step's entry at %08X is not as read, not timed", kWorldStep);
		mem::ReportForeignCode("Physics", kWorldStep);
		return false;
	}
	void* const buffer = mem::AllocExecutable(32);
	if (buffer == nullptr) {
		return false;
	}
	const UInt32 at = reinterpret_cast<UInt32>(buffer);
	if (mem::BuildEntryTrampoline(static_cast<UInt8*>(buffer), 32, at, kWorldStep, kWorldStepEntry,
	                              sizeof(kWorldStepEntry)) == 0) {
		return false;
	}
	t.trampoline = at;
	UInt8 patch[5];
	if (mem::BuildEntryPatch(patch, sizeof(patch), kWorldStep, reinterpret_cast<UInt32>(&TimedWorldStep),
	                         sizeof(patch)) != sizeof(patch) ||
	    !mem::SafeWrite(kWorldStep, patch, sizeof(patch))) {
		t.trampoline = 0;
		return false;
	}
	return true;
}

}  // namespace

void StepEvenPhysicsPlan(bool wanted) {
	PlanState& s = Plan();
	if (wanted != s.wanted) {
		OBVR_LOG("Physics: even steps %s", wanted ? "on (PhysicsEvenSteps)" : "off - the engine's own plan");
	}
	s.wanted = wanted;
	if (wanted && !s.tried) {
		s.tried = true;
		s.installed = InstallPlanner();
		OBVR_LOG("Physics: the planner's call at %08X %s", kPlannerCall,
		         s.installed ? "goes to OBVR's even plan" : "stays the engine's");
	}
}

void StepPhysicsStepTimes(bool on, const char* mark) {
	StepTimes& t = Times();
	if (on && !t.tried) {
		t.tried = true;
		OBVR_LOG("Physics: step times %s", InstallStepTimes() ? "on - every step of the world timed"
		                                                         : "could not be put in");
	}
	++t.frames;
	if (mark == nullptr || t.trampoline == 0) {
		return;
	}
	static const long long s_frequency = ReadPerformanceFrequency();
	const double perMs = s_frequency > 0 ? static_cast<double>(s_frequency) / 1000.0 : 1.0;
	const double ms = static_cast<double>(t.ticks) / perMs;
	OBVR_LOG("Physics: at mark %s - %u steps in %u frames since the last mark, %.2f ms in all: %.3f ms a step, the "
	         "longest %.3f ms, %.3f ms a frame",
	         mark, t.steps, t.frames, ms, t.steps > 0 ? ms / t.steps : 0.0, static_cast<double>(t.longest) / perMs,
	         t.frames > 0 ? ms / t.frames : 0.0);
	t.steps = 0;
	t.ticks = 0;
	t.longest = 0;
	t.frames = 0;
}

}  // namespace obvr::game
