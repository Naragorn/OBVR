#include "game/AnalogWalk.h"

#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"

namespace obvr::game {
namespace {

constexpr UInt32 kPaceFactor = 0x00B14E58;
constexpr UInt32 kWalkSpeed = 0x005E3590;
constexpr UInt32 kRunSpeed = 0x005E3750;
constexpr UInt32 kAlwaysRunOffset = 0x58B;
// sub esp,10h; push esi; mov esi,ecx; mov eax,[esi] - both getters' first bytes.
constexpr UInt8 kSpeedBytes[] = {0x83, 0xEC, 0x10, 0x56, 0x8B, 0xF1, 0x8B, 0x06};

using SpeedFn = float(__fastcall*)(void* self, void* edx);

// The call of the movement animation's update in the player's handler:
// `call 0x005FCAB0` at 0x00672E0D, its arguments the pace factor and the
// turn's (fld [0x00B14E58] at 0x00672E04). The handler clamps the factor to 1
// every frame before it (0x00671620, `if (1.0 < factor) factor = 1.0`), so a
// sprint's factor over 1 is handed on here instead.
constexpr UInt32 kAnimUpdateCall = 0x00672E0D;
constexpr UInt32 kAnimUpdate = 0x005FCAB0;
// thiscall (self, pace, turn), `ret 8` at 0x005FE2DF.
using AnimUpdateFn = void(__fastcall*)(void* self, void* edx, float pace, float turn);

bool g_sprintHooked = false;
UInt32 g_animNext = 0;  // what the call held before OBVR's sprint took it
bool g_sprintOverride = false;
float g_sprintPace = 1.0f;

bool g_ready = false;
bool g_written = false;
bool g_turning = false;
vr::AnalogStick g_stick;
UInt32 g_lines = 12;

float* Pace() { return reinterpret_cast<float*>(kPaceFactor); }

}  // namespace

// The player's movement animation update, wrapped at its one call in the
// player's handler: a sprint's pace over 1 instead of the clamped factor. The
// call is chained: OBVR's aim (game/AimAtSource.cpp) wraps the same call
// first, so whatever it held is called on.
void __fastcall OBVR_PlayerAnimUpdate(void* self, void* edx, float pace, float turn) {
	reinterpret_cast<AnimUpdateFn>(g_animNext)(self, edx, g_sprintOverride ? g_sprintPace : pace, turn);
}

void InstallAnalogWalk() {
	const UInt8* const site = reinterpret_cast<const UInt8*>(kAnimUpdateCall);
	if (site[0] == 0xE8) {
		const UInt32 next = kAnimUpdateCall + 5 + *reinterpret_cast<const UInt32*>(site + 1);
		const UInt32 to = reinterpret_cast<UInt32>(&OBVR_PlayerAnimUpdate) - (kAnimUpdateCall + 5);
		g_animNext = next;
		g_sprintHooked = next != 0 && mem::SafeWrite(kAnimUpdateCall + 1, &to, sizeof(to));
	}
	OBVR_LOG("AnalogWalk: the sprint's pace %s (the call %08X, chained to %08X; the game's %08X)",
	         g_sprintHooked ? "goes past the handler's clamp" : "NOT hooked - the sprint runs at the run's pace",
	         kAnimUpdateCall, g_animNext, kAnimUpdate);
	g_ready = mem::Verify(kWalkSpeed, kSpeedBytes, sizeof(kSpeedBytes)) &&
	          mem::Verify(kRunSpeed, kSpeedBytes, sizeof(kSpeedBytes));
	if (g_ready) {
		OBVR_LOG("AnalogWalk: the walk and run speeds (%08X, %08X) read as read - the left stick walks analog through "
		         "the pace factor %08X (now %.2f)",
		         kWalkSpeed, kRunSpeed, kPaceFactor, static_cast<double>(*Pace()));
	} else {
		OBVR_LOG("AnalogWalk: the speed getters %08X/%08X are not the game's own - the left stick walks as before",
		         kWalkSpeed, kRunSpeed);
	}
}

void StepAnalogWalk(const AnalogWalkRequest& request, float& paceOut) {
	paceOut = 1.0f;
	void* const player = *reinterpret_cast<void* const*>(addr::kPlayerPointer);
	if (!g_ready || player == nullptr || !request.active) {
		g_turning = false;
		g_sprintOverride = false;
		if (g_written) {
			g_written = false;
			*Pace() = 1.0f;
			if (g_lines > 0) {
				--g_lines;
				OBVR_LOG("AnalogWalk: not walking analog - the pace factor back to 1");
			}
		}
		return;
	}
	const float walk = reinterpret_cast<SpeedFn>(kWalkSpeed)(player, nullptr);
	const float run = reinterpret_cast<SpeedFn>(kRunSpeed)(player, nullptr);
	const float pace =
		request.sprinting
			? vr::SprintPace(request.sprintFactor)
			: vr::AnalogPaceFactor(request.stick.amount, request.running, request.forcedRun, request.runFrom, walk, run);
	static bool s_sprintWas = false;
	static UInt32 s_sprintLines = 16;
	if (request.sprinting != s_sprintWas && s_sprintLines > 0) {
		--s_sprintLines;
		OBVR_LOG("AnalogWalk: %s - the pace factor %.2f", request.sprinting ? "sprinting" : "the sprint over",
		         static_cast<double>(pace));
	}
	s_sprintWas = request.sprinting;
	// The sprint's factor over 1 goes past the handler's clamp (OBVR_PlayerAnimUpdate);
	// the engine's own factor is left at the run's 1.
	g_sprintOverride = request.sprinting && g_sprintHooked;
	g_sprintPace = pace;
	*Pace() = request.sprinting ? 1.0f : pace;
	paceOut = pace;
	g_stick = request.stick;
	g_turning = true;
	if (!g_written && g_lines > 0) {
		--g_lines;
		OBVR_LOG("AnalogWalk: walking analog - push %.2f, %s, walk %.1f run %.1f a second, the pace factor %.2f",
		         static_cast<double>(request.stick.amount),
		         request.forcedRun ? "a run asked" : (request.running ? "running" : "walking"),
		         static_cast<double>(walk), static_cast<double>(run), static_cast<double>(pace));
	}
	g_written = true;
}

bool PlayerAlwaysRuns() {
	const UInt8* const player = *reinterpret_cast<const UInt8* const*>(addr::kPlayerPointer);
	return player != nullptr && player[kAlwaysRunOffset] != 0;
}

void TurnAnalogStep(float* step) {
	if (g_turning && step != nullptr) {
		vr::AnalogTurnStep(step, g_stick);
	}
}

}  // namespace obvr::game
