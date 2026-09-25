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

struct NativeSettingEdit {
 const SettingDefinition* definition = nullptr;
 float value = 0;
 bool action = false;
 bool repaint = false;
 bool close = false;
};

// The native presentation uses the SAME definitions, bounds and formatters
// as the original menu. Decisions propose edits; the host saves before applying.
class NativeSettings {
public:
 UInt32 First() const { return m_first; }
 UInt32 Selected() const { return m_selected; }
 UInt32 Pages() const { return (SettingDefinitionCount()+kNativeSettingsRows-1)/kNativeSettingsRows; }
 const SettingDefinition* Row(UInt32 slot) const;
 bool CanResetSelected(const Config& config) const;
 NativeSettingEdit Click(int id, const Config& config);
private:
 UInt32 m_first=0;
 UInt32 m_selected=0;
};

struct NativeSettingWriter {
 virtual ~NativeSettingWriter() = default;
 virtual bool Save(const SettingDefinition&, const char* value) = 0;
 virtual void Recenter() = 0;
};
enum class NativeEditResult { None, Saved, Action, SaveFailed, Refused };
NativeEditResult CommitNativeEdit(const NativeSettingEdit&, Config&, NativeSettingWriter&);

// RunScriptLine2 uses console separators. Text is a format-string operand,
// so quotes/percent signs must be escaped, and command/trait injection refused.
bool NativeTextCommand(const char* trait, const char* text, char* out, UInt32 capacity);
bool NativeFloatCommand(const char* trait, int value, char* out, UInt32 capacity);
bool NativeRowTrait(UInt32 slot, const char* suffix, char* out, UInt32 capacity);
} // namespace obvr::ui
