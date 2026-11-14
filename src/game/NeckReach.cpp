#include "game/NeckReach.h"

#include "core/Log.h"

namespace obvr::game {

NeckRule& TheNeckRule() {
	static NeckRule s_rule;
	return s_rule;
}

void NeckTaken(NeckRule& rule, UInt32 actor, const NiBound& bound, const NiPoint3& head, bool leftOutByBound) {
	const float outside = HeadOutsideBound(bound.center, bound.radius, head);
	NoteNeckTaken(rule, outside, leftOutByBound);
	static UInt32 s_lines = 8;
	if (leftOutByBound && s_lines > 0) {
		--s_lines;
		OBVR_LOG("Necks: the bound left out %08X whose head is in reach - the head %.0f units outside its 3D's bound "
		         "(radius %.0f); taken in this checking call, missed in the calls between",
		         actor, static_cast<double>(outside), static_cast<double>(bound.radius));
	}
}

void SayNeckRule(NeckRule& rule) {
	if (!NeckRuleLineDue(rule)) {
		return;
	}
	OBVR_LOG("Necks: by the bound first - %u calls, %u of %u loaded actors asked for their bones, %u head(s) in "
	         "reach; %u calls asked all to check the rule: %u left out with a head in reach; a taken head at most "
	         "%.0f units outside its 3D's bound (the slack %.0f)",
	         rule.calls - rule.said, rule.asked, rule.loaded, rule.taken, rule.checks, rule.missed,
	         static_cast<double>(rule.mostOutside), static_cast<double>(kNeckBoundSlackUnits));
	NeckRuleLineSaid(rule);
}

}  // namespace obvr::game
