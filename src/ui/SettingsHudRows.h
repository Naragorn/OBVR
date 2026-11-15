#pragma once

// The settings table's rows for the HUD on the hands (ui/SettingsList.cpp):
// their readers, writers and the macro that spells the three rows of one
// element. Kept apart from the table, which is long enough on its own.

#include "ui/SettingsList.h"

namespace obvr::ui {

// The HUD on the hands has the same three rows for every element; one reader
// and writer per row, told the element by the template argument.
template <UInt32 E>
inline float HudPlaceRead(const Config& c) {
	return static_cast<float>(c.hands.handHud.element[E].place);
}
template <UInt32 E>
inline void HudPlaceWrite(Config& c, float v) {
	c.hands.handHud.element[E].place = vr::HudPlaceFromIndex(v);
}
template <UInt32 E>
inline float HudOpacityRead(const Config& c) {
	return c.hands.handHud.element[E].opacity;
}
template <UInt32 E>
inline void HudOpacityWrite(Config& c, float v) {
	c.hands.handHud.element[E].opacity = v;
}
template <UInt32 E>
inline float HudSizeRead(const Config& c) {
	return c.hands.handHud.element[E].size;
}
template <UInt32 E>
inline void HudSizeWrite(Config& c, float v) {
	c.hands.handHud.element[E].size = v;
}

#define OBVR_HUD_ELEMENT_ROWS(E, NAME, KEY)                                                              \
	{                                                                                                    \
		"HUD", NAME, "Put it on the HUD panel, a hand, the sky, the top or bottom of your view, or under your target",  \
		ItemKind::Number, 0.0f, static_cast<float>(vr::kHudPlaceCount - 1), 1.0f, 0, false,              \
		"HandHud", KEY "Place", &HudPlaceRead<E>, &HudPlaceWrite<E>,                                     \
		"", "", SettingAction::None, vr::kHudPlaceNames, vr::kHudPlaceCount,                             \
	},                                                                                                   \
	{                                                                                                    \
		"HUD", NAME " opacity", "How see-through it is. 1 is solid", ItemKind::Number, 0.0f, 1.0f, 0.05f, 2, false, \
		"HandHud", KEY "Opacity", &HudOpacityRead<E>, &HudOpacityWrite<E>,                               \
	},                                                                                                   \
	{                                                                                                    \
		"HUD", NAME " size", "How big it is", ItemKind::Number, 0.5f, 3.0f, 0.1f, 1, false,         \
		"HandHud", KEY "Size", &HudSizeRead<E>, &HudSizeWrite<E>,                                        \
	},

}  // namespace obvr::ui
