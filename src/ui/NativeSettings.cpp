#include "ui/NativeSettings.h"

namespace obvr::ui {
namespace {
bool Same(const char* a,const char* b) {
 while (*a && *a==*b) { ++a; ++b; }
 return *a==*b;
}
bool Key(const SettingDefinition& d,const char* section,const char* key) {
 const char* a=d.iniSection; const char* b=section;
 while (*a && *a==*b) { ++a; ++b; }
 if (*a!=*b) return false;
 a=d.iniKey; b=key;
 while (*a && *a==*b) { ++a; ++b; }
 return *a==*b;
}
}
const char* SettingsModeName(SettingsMode mode) {
 return mode==SettingsMode::FullVr ? "Full VR Port" : "VR View";
}
bool SettingRelevantIn(SettingsMode mode,const SettingDefinition& d) {
 const bool full=mode==SettingsMode::FullVr;
 // The switch between the two is in both.
 if (Key(d,"Hands","Enabled")) return true;
 // With Full VR off the controllers can still drive the menus, or play as a
 // gamepad: VR View's.
 if (Key(d,"Hands","ControllerMenus") || Key(d,"Hands","GamepadLayout")) return !full;
 // Everything else under Hands, and the teleport: the Full VR Port's.
 if (Same(d.category,"Hands") || Same(d.category,"Teleport")) return full;
 // Looking and aiming by the head, and the body under the headset: VR View's
 // (in Full VR the hands aim and the arms are the controllers).
 if (Same(d.category,"Looking") || Same(d.category,"Aiming") || Same(d.category,"Body"))
  return !full;
 return true;
}
bool SettingShownIn(SettingsView view,const SettingDefinition& d,const Config& c) {
 if (view==SettingsView::All || view==SettingsView::Sections) return true;
 const bool snap=c.look.snapTurning;
 if (Key(d,"Hands","LeftHanded") || Key(d,"Look","SnapTurning")) return true;
 if (Key(d,"Look","SnapTurnAngle") || Key(d,"Look","SnapTurnInstant") ||
     Key(d,"Look","SnapTurnVignette")) return snap;
 // Easing speed means nothing to an instant snap.
 if (Key(d,"Look","SnapTurnSpeed")) return snap && !c.look.snapTurnInstant;
 if (Key(d,"Look","SnapTurnVignetteRadius") || Key(d,"Look","SnapTurnVignetteStrength"))
  return snap && c.look.snapTurnVignette;
 return false;
}
NativeSettings::NativeSettings() {
 const UInt32 total=SettingDefinitionCount();
 m_count=total<kNativeMaxRows ? total : kNativeMaxRows;
 for (UInt32 i=0;i<m_count;++i) m_rows[i]=i;
}
void NativeSettings::SetMode(SettingsMode mode,const Config& config) {
 m_mode=mode;
 SetView(m_view,config);
}
void NativeSettings::SetView(SettingsView view,const Config& config) {
 m_view=view; m_first=0; m_count=0; m_section=-1;
 Sync(config);
 m_selected=m_count ? m_rows[0] : 0;
}
void NativeSettings::Sync(const Config& config) {
 const UInt32 total=SettingDefinitionCount();
 // The sections: each category once, in the order its rows first appear.
 m_sectionCount=0;
 if (m_view==SettingsView::Sections) {
  for (UInt32 i=0;i<total;++i) {
   if (!SettingRelevantIn(m_mode,SettingDefinitions()[i])) continue;
   const char* category=SettingDefinitions()[i].category;
   UInt32 k=0;
   while (k<m_sectionCount && !Same(m_sectionNames[k],category)) ++k;
   if (k==m_sectionCount) {
    if (m_sectionCount>=kNativeMaxSections) continue;
    m_sectionNames[m_sectionCount]=category;
    m_sectionSizes[m_sectionCount++]=0;
   }
   ++m_sectionSizes[k];
  }
  if (m_section>=static_cast<int>(m_sectionCount)) m_section=-1;
 }
 m_count=0;
 bool selectedShown=false;
 for (UInt32 i=0;i<total && m_count<kNativeMaxRows;++i) {
  const auto& d=SettingDefinitions()[i];
  if (!SettingShownIn(m_view,d,config)) continue;
  if (m_view==SettingsView::Sections &&
      (m_section<0 || !SettingRelevantIn(m_mode,d) || !Same(d.category,m_sectionNames[m_section])))
   continue;
  if (i==m_selected) selectedShown=true;
  m_rows[m_count++]=i;
 }
 const UInt32 last=(Pages()-1)*kNativeSettingsRows;
 if (m_first>last) m_first=last;
 if (!selectedShown) m_selected=m_first<m_count ? m_rows[m_first] : (m_count ? m_rows[0] : 0);
}
const char* NativeSettings::SectionAt(UInt32 slot) const {
 if (!InOverview() || slot>=kNativeSettingsRows || m_first+slot>=m_sectionCount) return nullptr;
 return m_sectionNames[m_first+slot];
}
UInt32 NativeSettings::SectionSize(UInt32 slot) const {
 return SectionAt(slot) ? m_sectionSizes[m_first+slot] : 0;
}
const SettingDefinition* NativeSettings::Row(UInt32 slot) const {
 if (InOverview()) return nullptr;
 if (slot>=kNativeSettingsRows || m_first+slot>=m_count) return nullptr;
 return &SettingDefinitions()[m_rows[m_first+slot]];
}
bool NativeSettings::CanResetSelected(const Config& config) const {
 if (InOverview() || m_count==0 || m_selected>=SettingDefinitionCount()) return false;
 return CanResetSetting(SettingDefinitions()[m_selected],config);
}
NativeSettingEdit NativeSettings::Click(int id,const Config& config) {
 NativeSettingEdit r;
 if (id==kNativeClose) { r.close=true; return r; }
 if (id==kNativeReset) {
  if (!CanResetSelected(config)) return r;
  const auto* definition=&SettingDefinitions()[m_selected];
  // CanResetSelected has already established that this definition has a
  // reader and differs from its canonical default. Read the same default
  // source directly here so the proposal has no second refusal branch.
  const Config defaults;
  const float defaultValue=definition->Read(defaults);
  r.definition=definition;
  r.value=defaultValue;
  r.repaint=true;
  return r;
 }
 if (id==kNativePrevious || id==kNativeNext) {
  const UInt32 last=(Pages()-1)*kNativeSettingsRows;
  m_first=id==kNativePrevious ? (m_first==0 ? last : m_first-kNativeSettingsRows)
                             : (m_first==last ? 0 : m_first+kNativeSettingsRows);
  m_selected=m_count ? m_rows[m_first] : 0;
  r.repaint=true;
  return r;
 }
 if (id==kNativeBack) {
  if (m_view!=SettingsView::Sections || m_section<0) return r;
  // Back to the list, on the page that shows the section just left.
  const UInt32 left=static_cast<UInt32>(m_section);
  m_section=-1;
  Sync(config);
  m_first=(left/kNativeSettingsRows)*kNativeSettingsRows;
  r.repaint=true;
  return r;
 }
 if (id<kNativeRowBase || id>=kNativeRowBase+static_cast<int>(kNativeSettingsRows)*3) return r;
 const UInt32 slot=static_cast<UInt32>(id-kNativeRowBase)/3;
 if (InOverview()) {
  // Any part of a section's row opens it.
  if (!SectionAt(slot)) return r;
  m_section=static_cast<int>(m_first+slot);
  m_first=0;
  Sync(config);
  m_selected=m_count ? m_rows[0] : 0;
  r.repaint=true;
  return r;
 }
 const auto* definition=Row(slot);
 if (!definition) return r;
 m_selected=m_rows[m_first+slot];
 r.repaint=true;
 const int part=(id-kNativeRowBase)%3;
 if (part==0) return r; // label selects help; it does not change a setting
 const auto item=ItemFor(*definition,config);
 if (item.kind==ItemKind::Action) {
  r.definition=definition; r.action=true; return r;
 }
 const float value=AdjustValue(item,part==1 ? MenuAction::Decrease : MenuAction::Increase);
 if (value!=item.value) { r.definition=definition; r.value=value; }
 return r;
}
NativeEditResult CommitNativeEdit(const NativeSettingEdit& edit,Config& config,NativeSettingWriter& writer) {
 if (!edit.definition) return NativeEditResult::None;
 if (edit.action) {
  if (edit.definition->action==SettingAction::Recenter) writer.Recenter();
  if (edit.definition->action==SettingAction::AdjustHands) writer.AdjustHands();
  if (edit.definition->action==SettingAction::FitHolsters) writer.FitHolsters();
  if (edit.definition->action==SettingAction::PlaceStowSpot) writer.PlaceStowSpot();
  return NativeEditResult::Action;
 }
 if (SettingEditRefusal(*edit.definition,config,edit.value)) return NativeEditResult::Refused;
 MenuItem item=ItemFor(*edit.definition,config);
 item.value=edit.value;
 char value[32];
 FormatValueForIni(item,edit.definition->falseWord,edit.definition->trueWord,value,sizeof(value));
 if (!writer.Save(*edit.definition,value)) return NativeEditResult::SaveFailed;
 ApplySetting(*edit.definition,config,edit.value);
 return NativeEditResult::Saved;
}

namespace {
struct Buffer {
 char* out; UInt32 capacity; UInt32 at=0; bool ok=true;
 void Add(char c) { if (at+1>=capacity) { ok=false; return; } out[at++]=c; }
 void Text(const char* s) { while (*s) Add(*s++); }
 bool End() { out[ok ? at : 0]='\0'; return ok; }
};
bool ValidTrait(const char* s) {
 if (!s || !*s) return false;
 while (*s) {
  const char c=*s++;
  if (!((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_' || c=='\\')) return false;
 }
 return true;
}
}
bool NativeTextCommand(const char* trait,const char* text,char* out,UInt32 capacity) {
 if (!out || !capacity) return false;
 out[0]='\0';
 // xOBSE tokenizes the operand; an empty value is discarded instead of applied.
 if (!ValidTrait(trait) || !text || !*text) return false;
 Buffer b{out,capacity};
 b.Text("SetMenuStringValue \""); b.Text(trait); b.Add('@');
 for (const char* p=text; *p; ++p) {
  if (*p=='%' ) b.Text("%%");
  else if (*p=='"') b.Text("%q");
  // The script compiler ends the line at a semicolon, quotes or not: every
  // help line with one failed to compile (2026-09-25 log, Hold to sneak).
  else if (*p==';') b.Add(',');
  else if (*p=='@' || *p=='|' || static_cast<unsigned char>(*p)<32) { out[0]='\0'; return false; }
  else b.Add(*p);
 }
 b.Text("\" 1011");
 return b.End();
}
bool NativeFloatCommand(const char* trait,int value,char* out,UInt32 capacity) {
 if (!out || !capacity) return false;
 out[0]='\0';
 if (!ValidTrait(trait)) return false;
 char number[16]; FormatInteger(value,number,sizeof(number));
 Buffer b{out,capacity}; b.Text("SetMenuFloatValue \""); b.Text(trait); b.Text("\" 1011 "); b.Text(number); return b.End();
}
bool NativeRowTrait(UInt32 slot,const char* suffix,char* out,UInt32 capacity) {
 if (!out || !capacity) return false;
 out[0]='\0';
 if (slot>=kNativeSettingsRows || !ValidTrait(suffix)) return false;
 Buffer b{out,capacity}; b.Text("parchment\\row"); b.Add(static_cast<char>('0'+slot)); b.Add('\\'); b.Text(suffix); return b.End();
}
}
