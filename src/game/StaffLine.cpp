#include "game/StaffLine.h"

#include "game/FirstPersonHide.h"
#include "game/GameTypes.h"
#include "game/MeleeHit.h"
#include "game/MeleeHits.h"
#include "game/ParryLogic.h"
#include "game/PlayerAim.h"

namespace obvr::game {
namespace {

constexpr SInt32 kWeaponTypeStaff = static_cast<SInt32>(WeaponTypeCode::Staff);

struct StaffLineState {
	bool read = false;
	NiPoint3 a{0.0f, 0.0f, 0.0f};
	NiPoint3 b{0.0f, 0.0f, 0.0f};
};

StaffLineState& State() {
	static StaffLineState s;
	return s;
}

}  // namespace

bool ReadStaffLine(NiPoint3& a, NiPoint3& b) {
	StaffLineState& s = State();
	s.read = false;
	SInt32 type = -1;
	if (EquippedWeaponForm(&type) != nullptr && type == kWeaponTypeStaff &&
	    ReadPlayerWeaponState() == WeaponState::Drawn) {
		const NiAVObject* const node = FindFirstPersonNode("Weapon");
		if (node != nullptr) {
			const NiMatrix33& w = node->worldTransform.rot;
			const NiPoint3 axis{w.data[0][1], w.data[1][1], w.data[2][1]};
			s.read = TheirBladeFromNode(node->worldTransform.pos, axis, node->worldBound.center, node->worldBound.radius,
			                            s.a, s.b);
		}
	}
	a = s.a;
	b = s.b;
	return s.read;
}

bool StaffLineRead() { return State().read; }

void LastStaffLine(NiPoint3& a, NiPoint3& b) {
	a = State().a;
	b = State().b;
}

}  // namespace obvr::game
