#pragma once
#include "ui/SettingsList.h"

namespace obvr::ui {
constexpr UInt32 kNativeSettingsRows = 7;
constexpr int kNativePrevious = 9201;
constexpr int kNativeNext = 9202;
constexpr int kNativeReset = 9203;
constexpr int kNativeClose = 9299;
constexpr int kNativeRowBase = 9300; // three IDs per slot: help, minus, plus

// A generic menu belongs to one mod at a time. Never process keyboard or
// click input while another menu owns the foreground or generic root.
enum class NativeSettingsStep { Wait, Release, Handle, Close, Open };
inline NativeSettingsStep SettingsStep(bool opened, bool root, bool foreign,
 bool foreground, bool toggle, bool escape, bool closeButton,
 bool loading, bool ready) {
 if (opened) {
  if (foreign || !root || closeButton) return NativeSettingsStep::Release;
  if (!foreground) return NativeSettingsStep::Wait;
  return toggle || escape ? NativeSettingsStep::Close : NativeSettingsStep::Handle;
 }
 return toggle && !root && !loading && ready ? NativeSettingsStep::Open : NativeSettingsStep::Wait;
}

// The comfort page after Full VR is chosen. The choice's own generic menu
// closes after the click, and until any generic menu is gone the page waits;
// then it opens over the main menu, or is dropped when something else is in
// front by then - a page nobody asked for never opens inside the game.
enum class ComfortPageStep { None, Wait, Open, Skip };
inline ComfortPageStep StepComfortPage(bool pending, bool genericRoot, bool mainMenuOnTop) {
 if (!pending) return ComfortPageStep::None;
 if (genericRoot) return ComfortPageStep::Wait;
 return mainMenuOnTop ? ComfortPageStep::Open : ComfortPageStep::Skip;
}

// The update notice, a small native window in the main menu with an OK that
// closes it. Once per session: it waits for the answer from GitHub, for the
// main menu to be in front and for the walkthrough, its comfort page and
// the settings to be done with - any generic menu up means wait - and once
// it has been up and is gone it never comes back. Never in the game.
enum class UpdateWindowStep { None, Wait, Open };
struct UpdateWindowState {
 bool shown=false;
 bool done=false;
};
inline UpdateWindowStep StepUpdateWindow(UpdateWindowState& s,bool updateKnown,bool mainMenuOnTop,
                                         bool genericRoot,bool busy) {
 if (s.done || !updateKnown) return UpdateWindowStep::None;
 if (s.shown) {
  if (!genericRoot) s.done=true;  // OK clicked, or closed some other way
  return s.done ? UpdateWindowStep::None : UpdateWindowStep::Wait;
 }
 if (!mainMenuOnTop) return UpdateWindowStep::None;
 if (genericRoot || busy) return UpdateWindowStep::Wait;
 s.shown=true;
 return UpdateWindowStep::Open;
}
constexpr int kNativeUpdateOk = 9401;

// Oblivion's XML booleans: &false; is 1 and &true; is 2, and any number
// other than 2 reads as false (UESP, Oblivion Mod:Oblivion XML/Entities;
// CS wiki, Operator Element). Index by the bool.
constexpr int kXmlBool[2] = {1, 2};

// Previous and Next only when there is another page to turn to.
inline bool NativePagingShown(UInt32 pages) { return pages > 1; }

struct NativeSettingEdit {
 const SettingDefinition* definition = nullptr;
 float value = 0;
 bool action = false;
 bool repaint = false;
 bool close = false;
};

// Which rows the menu offers. All is the Insert menu. Comfort is the
// onboarding's second page after Full VR is chosen: left-handed and the snap
// turn with its vignette, each follow-up only while the row it refines is on.
enum class SettingsView { All, Comfort };
bool SettingShownIn(SettingsView view, const SettingDefinition& definition, const Config& config);
constexpr UInt32 kNativeMaxRows = 160;

// The native presentation uses the SAME definitions, bounds and formatters
// as the original menu. Decisions propose edits; the host saves before applying.
class NativeSettings {
public:
 NativeSettings();
 SettingsView View() const { return m_view; }
 // Switches the view and starts it at its first row.
 void SetView(SettingsView view, const Config& config);
 // Re-reads which rows the view offers - after an edit, or an INI reload,
 // turned a row that others depend on. Keeps the page and the selection
 // where they can stay, moves them where they cannot.
 void Sync(const Config& config);
 UInt32 First() const { return m_first; }
 // An index into SettingDefinitions(), always one of the offered rows.
 UInt32 Selected() const { return m_selected; }
 UInt32 Pages() const { return m_count==0 ? 1 : (m_count+kNativeSettingsRows-1)/kNativeSettingsRows; }
 const SettingDefinition* Row(UInt32 slot) const;
 bool CanResetSelected(const Config& config) const;
 NativeSettingEdit Click(int id, const Config& config);
private:
 SettingsView m_view=SettingsView::All;
 UInt32 m_rows[kNativeMaxRows]{};
 UInt32 m_count=0;
 UInt32 m_first=0;
 UInt32 m_selected=0;
};

struct NativeSettingWriter {
 virtual ~NativeSettingWriter() = default;
 virtual bool Save(const SettingDefinition&, const char* value) = 0;
 virtual void Recenter() = 0;
 // The "Adjust hands" row: the settings menu closes and the guide opens.
 virtual void AdjustHands() {}
};
enum class NativeEditResult { None, Saved, Action, SaveFailed, Refused };
NativeEditResult CommitNativeEdit(const NativeSettingEdit&, Config&, NativeSettingWriter&);

// RunScriptLine2 uses console separators. Text is a format-string operand,
// so quotes/percent signs must be escaped, and command/trait injection refused.
bool NativeTextCommand(const char* trait, const char* text, char* out, UInt32 capacity);
bool NativeFloatCommand(const char* trait, int value, char* out, UInt32 capacity);
bool NativeRowTrait(UInt32 slot, const char* suffix, char* out, UInt32 capacity);
} // namespace obvr::ui
