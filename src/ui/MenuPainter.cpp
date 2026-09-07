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

// A row with a picture is this many lines tall, and the picture is drawn at
// twice the text's scale: a 16-row picture is then 32 font pixels, which sits
// inside five lines of nine with room to breathe.
constexpr UInt32 kIconLines = 5;
constexpr UInt32 kIconScale = 2;

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

UInt32 IconWidth(const MenuItem& item) {
	UInt32 widest = 0;
	for (UInt32 row = 0; row < item.iconRows; ++row) {
		UInt32 length = 0;
		while (item.icon[row] != nullptr && item.icon[row][length] != '\0') {
			++length;
		}
		if (length > widest) {
			widest = length;
		}
	}
	return widest;
}

UInt32 LinesOf(const MenuItem& item) {
	return item.icon != nullptr && item.iconRows > 0 ? kIconLines : 1;
}

// The border: the outer frame, a thin light line inset from it, and a
// diamond at each inner corner - vanilla's panels, in the strokes a pixel
// canvas has.
void PaintChrome(Canvas& canvas, SInt32 scale, const MenuTheme& theme) {
	const SInt32 width = static_cast<SInt32>(canvas.Width());
	const SInt32 height = static_cast<SInt32>(canvas.Height());
	canvas.DrawFrame(0, 0, width, height, scale, theme.frame);
	const SInt32 inset = scale * 3;
	canvas.DrawFrame(inset, inset, width - 2 * inset, height - 2 * inset, 1, theme.frameLight);
	const SInt32 radius = scale * 2;
	canvas.FillDiamond(inset, inset, radius, theme.frameLight);
	canvas.FillDiamond(width - 1 - inset, inset, radius, theme.frameLight);
	canvas.FillDiamond(inset, height - 1 - inset, radius, theme.frameLight);
	canvas.FillDiamond(width - 1 - inset, height - 1 - inset, radius, theme.frameLight);
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
	PaintMenu(canvas, items, categories, count, state, scale, theme, "OBVR settings");
}

void PaintMenu(Canvas& canvas, const MenuItem* items, const char* const* categories, UInt32 count,
               MenuState state, UInt32 scale, const MenuTheme& theme, const char* title) {
	if (scale == 0) {
		scale = 1;
	}

	const SInt32 s = static_cast<SInt32>(scale);
	const SInt32 margin = static_cast<SInt32>(kMargin * scale);
	const SInt32 lineHeight = static_cast<SInt32>(kLineAdvance * scale);
	const SInt32 width = static_cast<SInt32>(canvas.Width());
	const SInt32 height = static_cast<SInt32>(canvas.Height());

	canvas.Fill(theme.background);
	PaintChrome(canvas, s, theme);

	// The title, centred, with a rule under it that a diamond sits on - the
	// eye has somewhere to stop before the list starts.
	// One step larger than the rows: seven font pixels at scale + 1 still sit
	// inside the two lines the title owns at any scale.
	const char* const shown = title != nullptr ? title : "OBVR settings";
	const SInt32 titleScale = s + 1;
	const SInt32 titleWidth = static_cast<SInt32>(TextWidth(shown)) * titleScale;
	const SInt32 titleX = titleWidth < width - 2 * margin ? (width - titleWidth) / 2 : margin;
	canvas.DrawText(titleX, margin + s, shown, titleScale, theme.title);
	const SInt32 ruleY = margin + lineHeight + s * 5;
	canvas.FillRect(margin + s * 4, ruleY, width - 2 * margin - s * 8, 1, theme.frameLight);
	canvas.FillDiamond(width / 2, ruleY, s * 2, theme.frameLight);

	if (items == nullptr || count == 0) {
		return;
	}

	const UInt32 visibleRows = VisibleRowsFor(canvas.Height(), scale);

	// The line below which nothing from the list may be drawn. The help line
	// and the rule above it live there, and a row drawn into that space lands
	// on top of the text explaining it - which is what the first run in a
	// headset showed at the bottom of the list.
	const SInt32 contentBottom = height - margin - static_cast<SInt32>(kHelpLines) * lineHeight;

	// The state is used as given rather than re-derived. AdvanceMenu is what
	// keeps the selection and the window consistent, and a painter that
	// corrected them here would hide a fault in the one place it could be seen.
	// What it must not do is read past the array, so the loop is bounded by the
	// count and not by the window.
	SInt32 y = margin + kTitleLines * lineHeight;

	// Counted rather than derived from the loop index, because a category
	// heading takes a line too, and a row with a picture takes several. That
	// was the bug: headings were drawn without being counted, so a window
	// holding two of them pushed its last two rows past the bottom and into
	// the help line.
	UInt32 linesUsed = 0;

	for (UInt32 index = state.firstVisible; index < count && linesUsed < visibleRows; ++index) {
		const MenuItem& item = items[index];
		const UInt32 rowLines = LinesOf(item);
		const SInt32 rowHeight = static_cast<SInt32>(rowLines) * lineHeight;

		// The heading above the first row of each category.
		const bool firstOfCategory =
			categories != nullptr &&
			(index == 0 || !SameText(categories[index], categories[index - 1]));

		if (firstOfCategory && index > state.firstVisible) {
			// Room for the heading AND the row it introduces, or neither. A
			// heading alone at the bottom announces rows that are not on
			// screen, which reads as a menu that has lost its contents.
			if (linesUsed + 1 + rowLines > visibleRows ||
			    y + lineHeight + rowHeight > contentBottom) {
				break;
			}

			canvas.DrawText(margin + s * 2, y, categories[index], s, theme.category);
			y += lineHeight;
			++linesUsed;
		}

		// The last guard, and the one that makes the collision impossible
		// rather than merely unlikely: whatever the counting says, nothing is
		// drawn below the line the help text owns.
		if (y + rowHeight > contentBottom) {
			break;
		}

		const bool selected = index == state.selected;
		if (selected) {
			canvas.FillRect(margin + s * 2, y - s, width - 2 * margin - s * 4, rowHeight,
			                theme.highlight);
		}

		const render::Pixel labelColour =
			selected ? theme.highlightText
			         : (item.kind == ItemKind::Text ? theme.help : theme.text);
		const render::Pixel valueColour = selected ? theme.highlightText : theme.value;

		if (rowLines > 1) {
			// A picture on the left at twice the scale, the label and its help
			// beside it, centred on the row; the chosen one carries a diamond.
			const SInt32 iconScale = s * static_cast<SInt32>(kIconScale);
			const SInt32 iconHeight = static_cast<SInt32>(item.iconRows) * iconScale;
			const SInt32 iconY = y + (rowHeight - iconHeight) / 2;
			const SInt32 iconX = margin + s * 4;
			canvas.DrawIcon(iconX, iconY, item.icon, item.iconRows, iconScale,
			                selected ? theme.highlightText : theme.text, theme.value);
			const SInt32 textX =
				iconX + static_cast<SInt32>(IconWidth(item)) * iconScale + s * 6;
			const SInt32 labelY = y + (rowHeight - 2 * lineHeight) / 2;
			if (item.chosen) {
				canvas.FillDiamond(textX + s * 2, labelY + lineHeight / 2 - s, s * 2,
				                   selected ? theme.highlightText : theme.value);
			}
			canvas.DrawText(textX + s * 7, labelY, item.label, s, labelColour);
			canvas.DrawText(textX + s * 7, labelY + lineHeight, item.help, s,
			                selected ? theme.highlightText : theme.help);
			y += rowHeight;
			linesUsed += rowLines;
			continue;
		}

		SInt32 labelX = margin + s * 4;
		if (item.chosen) {
			canvas.FillDiamond(labelX + s, y + lineHeight / 2 - s, s,
			                   selected ? theme.highlightText : theme.value);
			labelX += s * 4;
		}
		canvas.DrawText(labelX, y, item.label, s, labelColour);

		char text[24];
		FormatValue(item, text, sizeof(text));

		// Right-aligned, which is what lets a column of numbers be compared at
		// a glance. TextWidth deliberately excludes the trailing gap, or every
		// value would sit one pixel short of where it says it does.
		const SInt32 valueRight = width - margin - s * 4;
		const SInt32 valueX = valueRight - static_cast<SInt32>(TextWidth(text) * scale);
		canvas.DrawText(valueX, y, text, s, valueColour);

		// A row whose value will not take effect until the next start says so
		// on the row. Somebody who changes a setting and sees nothing happen
		// concludes the menu is broken, not that this row is different.
		if (item.needsRestart) {
			const char* const mark = "*";
			canvas.DrawText(valueX - static_cast<SInt32>((TextWidth(mark) + 2) * scale), y, mark,
			                s, selected ? theme.highlightText : theme.warning);
		}

		y += lineHeight;
		++linesUsed;
	}

	// The help line for whatever is selected, along the bottom. One line,
	// because a headset is a bad place to read a paragraph - the paragraphs
	// stay in the INI, where there is room for them.
	if (state.selected < count) {
		const SInt32 helpY = height - margin - lineHeight;

		canvas.FillRect(margin + s * 4, helpY - s * 3, width - 2 * margin - s * 8, 1,
		                theme.frameLight);
		canvas.DrawText(margin + s * 2, helpY, items[state.selected].help, s,
		                items[state.selected].needsRestart ? theme.warning : theme.help);
	}
}

SInt32 RowAtPixel(const MenuItem* items, const char* const* categories, UInt32 count,
                  MenuState state, UInt32 canvasWidth, UInt32 canvasHeight, UInt32 scale,
                  SInt32 x, SInt32 y) {
	if (items == nullptr || count == 0 || canvasWidth == 0 || canvasHeight == 0) {
		return -1;
	}
	if (scale == 0) {
		scale = 1;
	}
	const SInt32 s = static_cast<SInt32>(scale);
	const SInt32 margin = static_cast<SInt32>(kMargin * scale);
	const SInt32 lineHeight = static_cast<SInt32>(kLineAdvance * scale);
	const SInt32 width = static_cast<SInt32>(canvasWidth);
	const SInt32 height = static_cast<SInt32>(canvasHeight);

	// Inside the highlight bar's span across, which is where a row is.
	if (x < margin + s * 2 || x >= width - margin - s * 2) {
		return -1;
	}

	const UInt32 visibleRows = VisibleRowsFor(canvasHeight, scale);
	const SInt32 contentBottom = height - margin - static_cast<SInt32>(kHelpLines) * lineHeight;
	SInt32 rowY = margin + kTitleLines * lineHeight;
	UInt32 linesUsed = 0;

	// The same walk as PaintMenu's, row for row, heading for heading, with
	// the same two guards against drawing into the help line.
	for (UInt32 index = state.firstVisible; index < count && linesUsed < visibleRows; ++index) {
		const UInt32 rowLines = LinesOf(items[index]);
		const SInt32 rowHeight = static_cast<SInt32>(rowLines) * lineHeight;
		const bool firstOfCategory =
			categories != nullptr &&
			(index == 0 || !SameText(categories[index], categories[index - 1]));
		if (firstOfCategory && index > state.firstVisible) {
			if (linesUsed + 1 + rowLines > visibleRows ||
			    rowY + lineHeight + rowHeight > contentBottom) {
				break;
			}
			rowY += lineHeight;
			++linesUsed;
		}
		if (rowY + rowHeight > contentBottom) {
			break;
		}
		// The highlight bar starts one scale pixel above the row's text.
		if (y >= rowY - s && y < rowY - s + rowHeight) {
			return static_cast<SInt32>(index);
		}
		rowY += rowHeight;
		linesUsed += rowLines;
	}
	return -1;
}

}  // namespace obvr::ui
