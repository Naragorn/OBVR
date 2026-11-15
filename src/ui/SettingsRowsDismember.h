// The settings table's dismemberment rows under "Combat: tuning", included
// inside the table (ui/SettingsList.cpp kSettings) at their place in it - not
// a header of its own, so no include guard: it is read once, there.

	{
		"Combat: tuning", "Skill from a head", "Extra skill progress for taking a head",
		ItemKind::Number, 0.0f, 50.0f, 1.0f, 0, false,
		"Hands", "DismemberHeadSkillUses",
		+[](const Config& c) { return static_cast<float>(c.hands.dismemberHeadSkillUses); },
		+[](Config& c, float v) { c.hands.dismemberHeadSkillUses = static_cast<int>(v); },
	},
	{
		"Combat: tuning", "Skill from a hand", "Extra skill progress for cutting off a hand",
		ItemKind::Number, 0.0f, 50.0f, 1.0f, 0, false,
		"Hands", "DismemberHandSkillUses",
		+[](const Config& c) { return static_cast<float>(c.hands.dismemberHandSkillUses); },
		+[](Config& c, float v) { c.hands.dismemberHandSkillUses = static_cast<int>(v); },
	},
	{
		"Combat: tuning", "Most severed parts",
		"How many cut-off heads and limbs can lie around. Past that, no new cuts",
		ItemKind::Number, 16.0f, 128.0f, 8.0f, 0, false,
		"Hands", "DismemberMostCuts",
		+[](const Config& c) { return static_cast<float>(c.hands.dismember.most); },
		+[](Config& c, float v) { c.hands.dismember.most = static_cast<int>(v); },
	},
