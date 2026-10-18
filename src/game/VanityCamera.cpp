#include "game/VanityCamera.h"

#include "core/Memory.h"
#include "game/GameAddresses.h"

namespace obvr::game {

bool ResetAutoVanityClock() {
	volatile float* const clock = reinterpret_cast<volatile float*>(addr::kVanityAutoIdleSeconds);
	const float seconds = *clock;
	// A clock of seconds: finite, not negative, under a day. Anything else
	// means the address is not what it is believed to be.
	if (!(seconds >= 0.0f) || !(seconds < 86400.0f)) {
		return false;
	}
	if (seconds != 0.0f) {
		*clock = 0.0f;
	}
	return true;
}

bool EngineVanityModeOn() {
	return *reinterpret_cast<const volatile UInt8*>(addr::kVanityModeFlag) != 0;
}

}  // namespace obvr::game
