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

bool g_ready = false;
bool g_written = false;
bool g_turning = false;
vr::AnalogStick g_stick;
UInt32 g_lines = 12;

float* Pace() { return reinterpret_cast<float*>(kPaceFactor); }

}  // namespace

void InstallAnalogWalk() {
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
		vr::AnalogPaceFactor(request.stick.amount, request.running, request.forcedRun, request.runFrom, walk, run);
	*Pace() = pace;
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
