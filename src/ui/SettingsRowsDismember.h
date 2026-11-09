// The settings table's dismemberment rows under "Combat: tuning", included
// inside the table (ui/SettingsList.cpp kSettings) at their place in it - not
// a header of its own, so no include guard: it is read once, there.

	{
		"Combat: tuning", "Skill for a head", "Extra skill uses for a head cut off or crushed",
		ItemKind::Number, 0.0f, 50.0f, 1.0f, 0, false,
		"Hands", "DismemberHeadSkillUses",
		+[](const Config& c) { return static_cast<float>(c.hands.dismemberHeadSkillUses); },
		+[](Config& c, float v) { c.hands.dismemberHeadSkillUses = static_cast<int>(v); },
	},
	{
		"Combat: tuning", "Skill for a hand", "Extra skill uses for a hand cut off",
		ItemKind::Number, 0.0f, 50.0f, 1.0f, 0, false,
		"Hands", "DismemberHandSkillUses",
		+[](const Config& c) { return static_cast<float>(c.hands.dismemberHandSkillUses); },
		+[](Config& c, float v) { c.hands.dismemberHandSkillUses = static_cast<int>(v); },
	},
	{
		"Combat: tuning", "Most cuts at once",
		"Heads, hands, arms and legs off at the same time (16 to 128); beyond it a cut is refused until bodies go - lowering it frees none already cut",
		ItemKind::Number, 16.0f, 128.0f, 8.0f, 0, false,
		"Hands", "DismemberMostCuts",
		+[](const Config& c) { return static_cast<float>(c.hands.dismember.most); },
		+[](Config& c, float v) { c.hands.dismember.most = static_cast<int>(v); },
	},
