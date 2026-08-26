#include "game/MenuMode.h"

#include "game/GameAddresses.h"

namespace obvr::game {

bool IsMenuMode() {
	using IsMenuModeFn = bool();
	auto* isMenuMode = reinterpret_cast<IsMenuModeFn*>(addr::kIsMenuMode);
	return isMenuMode();
}

}  // namespace obvr::game
