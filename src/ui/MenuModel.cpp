#include "ui/MenuModel.h"

namespace obvr::ui {

namespace {

// Ten to the power of a small number, as an integer. Used to move a value's
// decimals up into the whole part before it is turned into digits.
//
// Capped rather than allowed to run away: past nine decimals the product no
// longer fits, and a value scaled by a number that overflowed prints garbage
// with complete confidence.
SInt32 PowerOfTen(UInt32 exponent) {
	if (exponent > 9) {
		exponent = 9;
	}

	SInt32 result = 1;
	for (UInt32 at = 0; at < exponent; ++at) {
		result *= 10;
	}
	return result;
}

// Writes `text` into `out`, stopping at the buffer. Returns how many
// characters landed, so the caller can keep writing after them.
UInt32 Append(char* out, UInt32 size, UInt32 at, const char* text) {
	if (out == nullptr || size == 0) {
		return at;
	}

	// One byte is always kept back for the terminator, which is what makes
	// every path out of this file leave a usable string.
	while (*text != '\0' && at + 1 < size) {
		out[at] = *text;
		++at;
		++text;
	}
	return at;
}

// The digits of a non-negative whole number, written backwards into a scratch
// buffer and then reversed. Its own function because both FormatInteger and
// the fractional half of FormatValue need it, and because the reversal is
// exactly the kind of thing that is written twice and got wrong once.
UInt32 AppendDigits(char* out, UInt32 size, UInt32 at, SInt32 value, UInt32 minimumDigits) {
	char digits[12];
	UInt32 count = 0;

	if (value == 0) {
		digits[count] = '0';
		++count;
	}
	while (value > 0 && count < sizeof(digits)) {
		digits[count] = static_cast<char>('0' + (value % 10));
		value /= 10;
		++count;
	}

	// Leading zeros for a fraction: two decimals of 5 is ".05", not ".5".
	while (count < minimumDigits && count < sizeof(digits)) {
		digits[count] = '0';
		++count;
	}

	while (count > 0 && at + 1 < size) {
		--count;
		out[at] = digits[count];
		++at;
	}
	return at;
}

}  // namespace

MenuState AdvanceMenu(MenuState state, MenuAction action, UInt32 itemCount, UInt32 visibleRows) {
	if (itemCount == 0) {
		return MenuState{};
	}

	// A window of no rows would divide the scrolling arithmetic by nothing and
	// leaves no sensible answer anyway; one row is the smallest menu that can
	// still be used.
	if (visibleRows == 0) {
		visibleRows = 1;
	}

	// A selection past the end can arrive from a caller whose list got shorter
	// between frames - a category filter, say. Brought back in range before
	// anything is moved, rather than after, so the move starts from a real row.
	if (state.selected >= itemCount) {
		state.selected = itemCount - 1;
	}

	const UInt32 page = visibleRows > kPageContextRows ? visibleRows - kPageContextRows : 1;

	switch (action) {
		case MenuAction::Up:
			state.selected = state.selected == 0 ? itemCount - 1 : state.selected - 1;
			break;

		case MenuAction::Down:
			state.selected = state.selected + 1 >= itemCount ? 0 : state.selected + 1;
			break;

		// A page stops at the end rather than wrapping. Wrapping one row is
		// obviously a wrap; wrapping a page looks like the list jumped
		// somewhere at random, and the two do not have to behave alike.
		case MenuAction::PageUp:
			state.selected = state.selected > page ? state.selected - page : 0;
			break;

		case MenuAction::PageDown:
			state.selected =
				state.selected + page < itemCount ? state.selected + page : itemCount - 1;
			break;

		case MenuAction::None:
		case MenuAction::Decrease:
		case MenuAction::Increase:
			break;
	}

	// The window follows, by the least it can. Both directions are needed:
	// after a wrap from the top to the bottom, the selection is below the
	// window and above it in the same move.
	if (state.selected < state.firstVisible) {
		state.firstVisible = state.selected;
	}
	if (state.selected >= state.firstVisible + visibleRows) {
		state.firstVisible = state.selected - visibleRows + 1;
	}

	// A window hanging past the end of a list that is shorter than it is shows
	// blank rows below the last item, which reads as the menu having lost its
	// contents.
	if (itemCount <= visibleRows) {
		state.firstVisible = 0;
	} else if (state.firstVisible + visibleRows > itemCount) {
		state.firstVisible = itemCount - visibleRows;
	}

	return state;
}

float AdjustValue(const MenuItem& item, MenuAction action) {
	if (action != MenuAction::Decrease && action != MenuAction::Increase) {
		return item.value;
	}

	if (item.kind == ItemKind::Text || item.kind == ItemKind::Action) {
		return item.value;
	}

	if (item.kind == ItemKind::Toggle) {
		// Anything not clearly off is on, so a value that arrived from an INI
		// as 2 does not leave the switch in a third state nobody can name.
		return item.value != 0.0f ? 0.0f : 1.0f;
	}

	const float moved =
		action == MenuAction::Increase ? item.value + item.step : item.value - item.step;

	if (moved < item.minimum) {
		return item.minimum;
	}
	if (moved > item.maximum) {
		return item.maximum;
	}
	return moved;
}

void FormatInteger(SInt32 value, char* out, UInt32 size) {
	if (out == nullptr || size == 0) {
		return;
	}

	UInt32 at = 0;
	if (value < 0) {
		at = Append(out, size, at, "-");

		// Negated after the sign is written, and guarded: the most negative
		// integer has no positive counterpart, and negating it lands back on
		// itself with the sign still set, which then prints as a run of
		// nonsense digits.
		if (value == -2147483647 - 1) {
			at = Append(out, size, at, "2147483648");
			out[at] = '\0';
			return;
		}
		value = -value;
	}

	at = AppendDigits(out, size, at, value, 1);
	out[at] = '\0';
}

void FormatValueForIni(const MenuItem& item, const char* falseWord, const char* trueWord,
                       char* out, UInt32 size) {
	if (out == nullptr || size == 0) {
		return;
	}

	if (item.kind == ItemKind::Toggle) {
		const bool on = item.value != 0.0f;

		// A worded setting, where Config's own reader wants "cinema" or
		// "world" rather than a digit. Only used when both words are given -
		// half a pair is a table row somebody was in the middle of editing,
		// and falling back to the number keeps the file readable.
		if (falseWord != nullptr && trueWord != nullptr && falseWord[0] != '\0' &&
		    trueWord[0] != '\0') {
			const UInt32 at = Append(out, size, 0, on ? trueWord : falseWord);
			out[at] = '\0';
			return;
		}

		const UInt32 at = Append(out, size, 0, on ? "1" : "0");
		out[at] = '\0';
		return;
	}

	// A number is written the way it is shown. The INI's own readers accept
	// what FormatValue produces - atof for a float, digits for an integer -
	// so there is nothing to translate.
	FormatValue(item, out, size);
}

void FormatValue(const MenuItem& item, char* out, UInt32 size) {
	if (out == nullptr || size == 0) {
		return;
	}

	if (item.kind == ItemKind::Text) {
		out[0] = '\0';
		return;
	}
	if (item.kind == ItemKind::Action) {
		const UInt32 at = Append(out, size, 0, ">");
		out[at] = '\0';
		return;
	}

	if (item.kind == ItemKind::Toggle) {
		const UInt32 at = Append(out, size, 0, item.value != 0.0f ? "on" : "off");
		out[at] = '\0';
		return;
	}

	const SInt32 scale = PowerOfTen(item.decimals);

	// Scaled and rounded in one step, half away from zero. Done on the value
	// as a whole rather than on its fractional part alone, so that a value
	// which rounds up into the next whole number carries properly - 0.999 at
	// two decimals is 1.00, not 0.100.
	const float scaled = item.value * static_cast<float>(scale);
	const float rounded = scaled >= 0.0f ? scaled + 0.5f : scaled - 0.5f;

	// Outside what an integer can hold there is nothing sensible to print, and
	// the conversion below would be undefined rather than merely wrong.
	if (!(rounded > -2147483000.0f && rounded < 2147483000.0f)) {
		const UInt32 at = Append(out, size, 0, "-");
		out[at] = '\0';
		return;
	}

	SInt32 whole = static_cast<SInt32>(rounded);

	UInt32 at = 0;
	if (whole < 0) {
		at = Append(out, size, at, "-");
		whole = -whole;
	}

	at = AppendDigits(out, size, at, whole / scale, 1);

	if (item.decimals > 0) {
		at = Append(out, size, at, ".");
		at = AppendDigits(out, size, at, whole % scale, item.decimals);
	}

	out[at] = '\0';
}

}  // namespace obvr::ui
