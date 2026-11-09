#include "game/HandBlood.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/Blood.h"
#include "game/CutMarkLogic.h"
#include "game/CutMarks.h"
#include "game/GameAddresses.h"

namespace obvr::game {
namespace {

UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }

// Laid from this far above the hand, straight down onto it.
constexpr float kAboveUnits = 12.0f;

}  // namespace

UInt32 StainPlayerHands(const NiPoint3 hands[2], const bool valid[2]) {
	const UInt32 player = Read(addr::kPlayerPointer);
	if (!mem::LooksLikeObjectAddress(player)) {
		return 0;
	}
	const char* const texture = BloodDecalTexture(player);
	if (texture == nullptr) {
		return 0;
	}
	const UInt32 roots[2] = {Read(player + addr::kRefNiNodeOffset), Read(player + addr::kPlayerFirstPersonNodeOffset)};
	static UInt32 s_lines = 8;
	UInt32 gained = 0;
	for (UInt32 h = 0; h < 2; ++h) {
		if (!valid[h]) {
			continue;
		}
		CutMark mark;
		mark.place = true;
		mark.from = hands[h] + NiPoint3{0.0f, 0.0f, kAboveUnits};
		mark.direction = NiPoint3{0.0f, 0.0f, -1.0f};
		mark.texture = texture;
		for (UInt32 r = 0; r < 2; ++r) {
			if (!mem::LooksLikeObjectAddress(roots[r])) {
				continue;
			}
			const UInt32 before = DecalsUnderNode(roots[r]);
			char why[256] = "";
			LayCutMarkOn(player, roots[r], mark, hands[h], why, sizeof(why));
			const UInt32 after = DecalsUnderNode(roots[r]);
			gained += after > before ? after - before : 0;
			if (s_lines > 0) {
				--s_lines;
				OBVR_LOG("Walkers disguise: blood at the %s hand on the %s - %s", h == 0 ? "left" : "right",
				         r == 0 ? "body" : "first-person arms", why);
			}
		}
	}
	return gained;
}

}  // namespace obvr::game
