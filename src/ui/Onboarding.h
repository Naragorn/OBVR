#pragma once

#include "core/Config.h"
#include "ui/MenuModel.h"
#include "ui/SettingsList.h"

namespace obvr::ui {

// The first-start walkthrough: a few pages in the headset that open on the
// choice between OBVR's two shapes - the seated experience, Oblivion as it
// is with keyboard, mouse or gamepad, and the standing one, a full VR port
// with motion controllers - then say how each is steered, offer a couple of
// comfort settings, and end on "do not show this again".
//
// The same machinery as the settings menu: rows the painter already knows
// how to draw, settings written through the same table, so a choice made
// here is the same INI key the settings menu changes later. Holds no
// Direct3D and no Windows; the caller feeds it MenuActions and the layer
// draws whatever BuildRows hands back, so every page and every key press
// can be walked through in a test.
//
// A page is a list of rows: explanatory text (not selectable), settings by
// INI section and key, choices with a picture that set a setting and turn
// the page, and actions - Back, Next, Finish. The highlight skips text
// rows; Left or Right on a setting changes it, on a choice or an action
// fires it.

enum class OnboardingRowKind { Text, Setting, Choice, Action };
enum class OnboardingAction { None, Back, Next, Finish, ChooseSeated, ChooseStanding };

struct OnboardingRow {
	OnboardingRowKind kind = OnboardingRowKind::Text;
	const char* text = "";
	const char* help = "";
	const char* iniSection = "";
	const char* iniKey = "";
	OnboardingAction action = OnboardingAction::None;
	const char* const* icon = nullptr;
	UInt32 iconRows = 0;
};

struct OnboardingPage {
	const char* title = "";
	const OnboardingRow* rows = nullptr;
	UInt32 rowCount = 0;
};

const OnboardingPage* OnboardingPages();
UInt32 OnboardingPageCount();

// Whether a choice row is the one in force for this configuration: seated
// while hand tracking is off, standing while it is on.
bool ChoiceIsCurrent(OnboardingAction choice, const Config& config);

// True while the standing experience (the hand-tracked mode) is under
// construction: the welcome page still shows the choice so people know it
// is coming, but pressing it does nothing - the mode stays off, the page
// stays. OBVR.ini and the settings menu can still switch it on.
bool StandingUnderConstruction();

class OnboardingMenu {
public:
	bool IsOpen() const { return m_open; }
	UInt32 Page() const { return m_page; }
	const char* Title() const;

	// Opens on the first page with the first selectable row highlighted.
	void Open();
	void Close();

	void SetVisibleRows(UInt32 rows);

	// One key press. Movement skips text rows; Left or Right on a setting
	// changes it and answers the definition to write back; on a choice it
	// writes the mode and turns the page, answering the mode's definition;
	// on an action it turns the page, and Finish closes the walkthrough.
	// Anything that changes the picture bumps the revision.
	const SettingDefinition* Apply(MenuAction action, Config& config);

	// Puts the highlight on a row a pointer is over. Text rows are not
	// selectable and are refused, as they are for the arrow keys. Answers
	// whether the picture changed.
	bool Hover(UInt32 index);

	// The current page as rows for the painter. Text rows come out as
	// ItemKind::Text, choices and actions as ItemKind::Action - the choices
	// with their picture and the current one marked - and settings as the
	// table's own items. Categories are the page title, so the painter
	// shows it as the heading of the list.
	UInt32 BuildRows(const Config& config, MenuItem* items, const char** categories,
	                 UInt32 capacity) const;

	MenuState State() const { return m_state; }
	UInt32 Revision() const { return m_revision; }

private:
	const OnboardingPage& CurrentPage() const;
	UInt32 FirstSelectable(const OnboardingPage& page) const;
	UInt32 StepSelection(const OnboardingPage& page, UInt32 from, bool down) const;
	void TurnTo(UInt32 page);

	bool m_open = false;
	UInt32 m_page = 0;
	MenuState m_state;
	UInt32 m_revision = 1;
	UInt32 m_visibleRows = 10;
};

}  // namespace obvr::ui
