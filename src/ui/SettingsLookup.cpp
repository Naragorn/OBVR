// The settings table's lookups and edits (ui/SettingsList.h), kept apart
// from the table itself (SettingsList.cpp), which is long enough on its own.
#include "ui/SettingsList.h"

namespace obvr::ui {

namespace {

// Two short strings, compared the only way a freestanding build can.
bool SameKey(const char* a, const char* b) {
	if (a == nullptr || b == nullptr) {
		return false;
	}
	while (*a != '\0' && *a == *b) {
		++a;
		++b;
	}
	return *a == *b;
}

float Clamp(const SettingDefinition& definition, float value) {
	if (value < definition.minimum) {
		return definition.minimum;
	}
	if (value > definition.maximum) {
		return definition.maximum;
	}
	return value;
}

}  // namespace


const SettingDefinition* FindSetting(const char* iniSection, const char* iniKey) {
	const SettingDefinition* const all = SettingDefinitions();
	const UInt32 count = SettingDefinitionCount();
	for (UInt32 i = 0; i < count; ++i) {
		if (SameKey(all[i].iniSection, iniSection) && SameKey(all[i].iniKey, iniKey)) {
			return &all[i];
		}
	}
	return nullptr;
}


MenuItem ItemFor(const SettingDefinition& definition, const Config& config) {
	MenuItem item;
	item.label = definition.label;
	item.help = definition.help;
	item.kind = definition.kind;
	item.minimum = definition.minimum;
	item.maximum = definition.maximum;
	item.step = definition.step;
	item.decimals = definition.decimals;
	item.valueNames = definition.valueNames;
	item.valueNameCount = definition.valueNameCount;
	item.needsRestart = definition.needsRestart;

	// A row with no reader is a half-finished table entry. Showing it at its
	// minimum makes that visible in the menu; reading through the null pointer
	// would make it visible in a crash dump.
	item.value = definition.Read != nullptr ? definition.Read(config) : definition.minimum;

	// A value already outside its range - carried in from a hand-edited INI -
	// is shown as what it is rather than silently corrected, because correcting
	// it here would write it back on the next frame without anyone asking. The
	// clamp belongs on the way in from a keypress, which is where it is.
	return item;
}

void ApplySetting(const SettingDefinition& definition, Config& config, float value) {
	if (definition.Write == nullptr) {
		return;
	}
	definition.Write(config, Clamp(definition, value));
}

bool CanonicalDefaultValue(const SettingDefinition& definition, float& value) {
	if (definition.Read == nullptr) {
		return false;
	}

	const Config defaults;
	value = definition.Read(defaults);
	return true;
}

bool CanResetSetting(const SettingDefinition& definition, const Config& config) {
	if (definition.kind == ItemKind::Action || definition.kind == ItemKind::Text ||
	    definition.Read == nullptr || definition.Write == nullptr) {
		return false;
	}

	float defaultValue = 0.0f;
	return CanonicalDefaultValue(definition, defaultValue) &&
	       definition.Read(config) != defaultValue;
}

namespace {

bool KeyIs(const char* key, const char* wanted) {
	if (key == nullptr) {
		return false;
	}
	while (*key != '\0' && *wanted != '\0' && *key == *wanted) {
		++key;
		++wanted;
	}
	return *key == '\0' && *wanted == '\0';
}

}  // namespace

const char* SettingEditRefusal(const SettingDefinition& definition, const Config& config,
                               float value) {
	const bool off = value == 0.0f;
	if (off && KeyIs(definition.iniKey, "LaserBeam") && !config.hands.laserDot) {
		return "The laser needs the beam or the dot: switch the dot on first.";
	}
	if (off && KeyIs(definition.iniKey, "LaserDot") && !config.hands.laserBeam) {
		return "The laser needs the beam or the dot: switch the beam on first.";
	}
	return nullptr;
}

}  // namespace obvr::ui
