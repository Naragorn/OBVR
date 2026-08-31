#include "ui/MenuPainter.h"

#include "ui/MenuFont.h"

namespace obvr::ui {

namespace {

// The margin round the panel's contents, in font pixels before scaling. Kept
// in the same units as everything else so that changing the scale moves the
// whole layout together rather than leaving the text creeping towards an edge
// that stayed put.
constexpr UInt32 kMargin = 4;

// The rows given over to the title at the top and the help line at the bottom,
// measured in line advances.
constexpr UInt32 kTitleLines = 2;
constexpr UInt32 kHelpLines = 2;

constexpr UInt32 kChromeLines = kTitleLines + kHelpLines;

bool SameText(const char* a, const char* b) {
	if (a == nullptr || b == nullptr) {
		return a == b;
	}
	while (*a != '\0' && *b != '\0') {
		if (*a != *b) {
			return false;
		}
		++a;
		++b;
	}
	return *a == *b;
}

}  // namespace

UInt32 VisibleRowsFor(UInt32 canvasHeight, UInt32 scale) {
	if (scale == 0) {
		scale = 1;
	}

	const UInt32 lineHeight = kLineAdvance * scale;
	const UInt32 chrome = kChromeLines * lineHeight + 2 * kMargin * scale;

	if (canvasHeight <= chrome) {
		return 1;
	}

	const UInt32 rows = (canvasHeight - chrome) / lineHeight;
	return rows == 0 ? 1 : rows;
}

void PaintMenu(Canvas& canvas, const MenuItem* items, const char* const* categories, UInt32 count,
               MenuState state, UInt32 scale, const MenuTheme& theme) {
	if (scale == 0) {
		scale = 1;
	}

	const SInt32 margin = static_cast<SInt32>(kMargin * scale);
	const SInt32 lineHeight = static_cast<SInt32>(kLineAdvance * scale);
	const SInt32 width = static_cast<SInt32>(canvas.Width());
	const SInt32 height = static_cast<SInt32>(canvas.Height());

	canvas.Fill(theme.background);
	canvas.DrawFrame(0, 0, width, height, static_cast<SInt32>(scale), theme.frame);

	canvas.DrawText(margin, margin, "OBVR settings", static_cast<SInt32>(scale), theme.title);

	// A rule under the title, so the eye has somewhere to stop before the list
	// starts. One pixel at the current scale, like the frame.
	canvas.FillRect(margin, margin + lineHeight + static_cast<SInt32>(scale) * 2,
	                width - 2 * margin, static_cast<SInt32>(scale), theme.frame);

	if (items == nullptr || count == 0) {
		return;
	}

	const UInt32 visibleRows = VisibleRowsFor(canvas.Height(), scale);

	// The state is used as given rather than re-derived. AdvanceMenu is what
	// keeps the selection and the window consistent, and a painter that
	// corrected them here would hide a fault in the one place it could be seen.
	// What it must not do is read past the array, so the loop is bounded by the
	// count and not by the window.
	SInt32 y = margin + kTitleLines * lineHeight;

	for (UInt32 row = 0; row < visibleRows; ++row) {
		const UInt32 index = state.firstVisible + row;
		if (index >= count) {
			break;
		}

		const MenuItem& item = items[index];

		// The heading above the first row of each category. Drawn in the row's
		// own space rather than in a line of its own, because a headset is
		// short of rows and a heading that costs a row costs a setting.
		const bool firstOfCategory =
			categories != nullptr &&
			(index == 0 || !SameText(categories[index], categories[index - 1]));

		if (firstOfCategory && row > 0) {
			canvas.DrawText(margin, y, categories[index], static_cast<SInt32>(scale),
			                theme.category);
			y += lineHeight;

			// A heading at the very bottom with nothing under it is a heading
			// for rows that are not on screen, which reads as a menu that has
			// lost its contents.
			if (y + lineHeight > height - margin - static_cast<SInt32>(kHelpLines) * lineHeight) {
				break;
			}
		}

		const bool selected = index == state.selected;
		if (selected) {
			canvas.FillRect(margin, y - static_cast<SInt32>(scale), width - 2 * margin,
			                lineHeight, theme.highlight);
		}

		const render::Pixel labelColour = selected ? theme.highlightText : theme.text;
		const render::Pixel valueColour = selected ? theme.highlightText : theme.value;

		canvas.DrawText(margin + static_cast<SInt32>(scale) * 2, y, item.label,
		                static_cast<SInt32>(scale), labelColour);

		char text[24];
		FormatValue(item, text, sizeof(text));

		// Right-aligned, which is what lets a column of numbers be compared at
		// a glance. TextWidth deliberately excludes the trailing gap, or every
		// value would sit one pixel short of where it says it does.
		const SInt32 valueRight = width - margin - static_cast<SInt32>(scale) * 2;
		const SInt32 valueX =
			valueRight - static_cast<SInt32>(TextWidth(text) * scale);
		canvas.DrawText(valueX, y, text, static_cast<SInt32>(scale), valueColour);

		// A row whose value will not take effect until the next start says so
		// on the row. Somebody who changes a setting and sees nothing happen
		// concludes the menu is broken, not that this row is different.
		if (item.needsRestart) {
			const char* const mark = "*";
			canvas.DrawText(valueX - static_cast<SInt32>((TextWidth(mark) + 2) * scale), y, mark,
			                static_cast<SInt32>(scale),
			                selected ? theme.highlightText : theme.warning);
		}

		y += lineHeight;
	}

	// The help line for whatever is selected, along the bottom. One line,
	// because a headset is a bad place to read a paragraph - the paragraphs
	// stay in the INI, where there is room for them.
	if (state.selected < count) {
		const SInt32 helpY = height - margin - lineHeight;

		canvas.FillRect(margin, helpY - static_cast<SInt32>(scale) * 3, width - 2 * margin,
		                static_cast<SInt32>(scale), theme.frame);
		canvas.DrawText(margin, helpY, items[state.selected].help, static_cast<SInt32>(scale),
		                items[state.selected].needsRestart ? theme.warning : theme.help);
	}
}

}  // namespace obvr::ui
