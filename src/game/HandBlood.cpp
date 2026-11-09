#include "game/HandBlood.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/Blood.h"
#include "game/CutMarkLogic.h"
#include "game/CutMarks.h"
#include "game/FirstPersonHide.h"
#include "game/GameAddresses.h"
#include "vr/WalkersDisguise.h"

namespace obvr::game {
namespace {

UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }

// Laid from this far outside the hand's middle, towards it.
constexpr float kOutsideUnits = 12.0f;

const char* const kHandBones[2] = {"Bip01 L Hand", "Bip01 R Hand"};
const char* const kMiddleFingers[2] = {"Bip01 L Finger2", "Bip01 R Finger2"};

}  // namespace

UInt32 StainPlayerHands(const NiPoint3& eye, const bool want[2], UInt32 side) {
	const UInt32 player = Read(addr::kPlayerPointer);
	if (!mem::LooksLikeObjectAddress(player)) {
		return 0;
	}
	const char* const texture = BloodDecalTexture(player);
	const UInt32 arms = Read(player + addr::kPlayerFirstPersonNodeOffset);
	if (texture == nullptr || !mem::LooksLikeObjectAddress(arms)) {
		return 0;
	}
	static UInt32 s_lines = 16;
	UInt32 gained = 0;
	for (UInt32 h = 0; h < 2; ++h) {
		if (!want[h]) {
			continue;
		}
		const NiAVObject* const bone = FindFirstPersonNode(kHandBones[h]);
		if (bone == nullptr) {
			continue;
		}
		const NiAVObject* const finger = FindFirstPersonNode(kMiddleFingers[h]);
		const NiPoint3 middle = finger != nullptr ? (bone->worldTransform.pos + finger->worldTransform.pos) * 0.5f
		                                          : bone->worldTransform.pos;
		CutMark mark;
		mark.place = true;
		mark.direction = vr::HandStainDirection(side, bone->worldTransform.rot, middle - eye);
		mark.from = middle - mark.direction * kOutsideUnits;
		mark.texture = texture;
		const UInt32 before = DecalsUnderNode(arms);
		char why[256] = "";
		LayCutMarkOn(player, arms, mark, middle, why, sizeof(why));
		const UInt32 after = DecalsUnderNode(arms);
		gained += after > before ? after - before : 0;
		if (s_lines > 0) {
			--s_lines;
			OBVR_LOG("Walkers disguise: blood at the %s hand from side %u (%.2f %.2f %.2f) - %s", h == 0 ? "left" : "right",
			         side, static_cast<double>(mark.direction.x), static_cast<double>(mark.direction.y),
			         static_cast<double>(mark.direction.z), why);
		}
	}
	return gained;
}

}  // namespace obvr::game
