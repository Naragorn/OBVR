#include "ui/NativeSettings.h"

namespace obvr::ui {
namespace {
bool Same(const char* a,const char* b) {
 while (*a && *a==*b) { ++a; ++b; }
 return *a==*b;
}
struct GuideRow { const char* section; const char* key; };
// Every row SettingShownIn can offer on the comfort page, in the page's order
// (Vanilla VR's real height first, its eased mouse turn after the snap's rows).
const GuideRow kComfortOrder[]={
 {"Head","RealHeight"},
 {"Look","SnapTurning"},{"Look","SnapTurnAngle"},{"Look","SnapTurnInstant"},{"Look","SnapTurnSpeed"},
 {"Look","SnapTurnVignette"},{"Look","SnapTurnVignetteRadius"},{"Look","SnapTurnVignetteStrength"},{"Look","SmoothTurning"},
 {"Hands","LeftHanded"},{"Hands","SneakIndicator"},{"Hands","TurnSpeed"},{"Hands","WalkDirection"},
 {"Tablet","Enabled"},{"Tablet","Figure"},{"Tablet","Unroll"},
 {"Locomotion","TeleportInstant"},{"Locomotion","TeleportVignette"},
 {"Hands","ControllerMenus"},{"Look","FirstPersonAnimations"},{"Look","NoPlayerKnockdown"},
 {"Look","ExhaustionSounds"},{"Look","ExhaustionHeavyWeapon"},{"Onboarding","Hints"},{"Onboarding","StartGuide"},
};
// The combat page's rows in its order: the kick, how the foes fight, the cuts.
const GuideRow kCombatOrder[]={
 {"Hands","Kick"},{"Hands","CombatRing"},{"Hands","EnemyAttackSpeed"},
 {"Hands","Dismemberment"},{"Hands","DismemberBlood"},{"Hands","DismemberCaps"},
};
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
 return mode==SettingsMode::FullVr ? "Full VR" : "Vanilla VR";
}
bool SettingRelevantIn(SettingsMode mode,const SettingDefinition& d) {
 const bool full=mode==SettingsMode::FullVr;
 // The switch between the two is in both.
 if (Key(d,"Hands","Enabled")) return true;
 // With Full VR off the controllers can still drive the menus, or play as a
 // gamepad: VR View's.
 if (Key(d,"Hands","ControllerMenus") || Key(d,"Hands","GamepadLayout")) return !full;
 // The hints in play sit over the hands: the Full VR Port's (camera/hands/Hints.h).
 if (Key(d,"Onboarding","Hints")) return full;
 // Everything in [Hands] and [Locomotion] (the teleport), and the buttons that
 // fit the hands, the stow spot and the weapon places: the Full VR Port's. By
 // the row's INI home, not its heading - the headings group by topic (2026-11-07),
 // and a topic such as Moving holds rows of both modes.
 if (Same(d.iniSection,"Hands") || Same(d.iniSection,"Locomotion") || d.action==SettingAction::AdjustHands ||
     d.action==SettingAction::PlaceStowSpot || d.action==SettingAction::FitHolsters ||
     d.action==SettingAction::ShowHintsAgain) return full;
 // Looking and aiming by the head: VR View's (in Full VR the hands aim).
 // The body under the headset is both modes' - its arms follow the hands in
 // Full VR (the body IK), and "Arm stretch", which the T-pose sets, was out
 // of Full VR's reach (Nadi, 2026-10-22: "ich bin mir sicher das wir
 // armlength oder sowas in den settings haben" - it was, under Body, in VR
 // View's menu only).
 if (Same(d.category,"Looking") || Same(d.category,"Aiming"))
  return !full;
 // Ease the turn and its speed: the mouse's or gamepad's turn, VR View's.
 // Full VR's stick has its own speed and snap rows, and the eased turn is
 // left out there (camera::TurnEases; Nadi, 2026-10-22).
 if (Key(d,"Look","SmoothTurning") || Key(d,"Look","TurnSpeed")) return !full;
 return true;
}
// The combat page: the kick takes the stick's flick up, which otherwise jumps -
// a surprise worth asking about first; foes taking turns and how fast they
// swing - the first thing to turn down for a player new to fighting with
// their arms; and the cuts, taste, with their blood and wounds while the
// cuts are on. All Full VR's.
static bool ShownOnCombatPage(const SettingDefinition& d,const Config& c) {
 if (!c.fullVrMode) return false;
 if (Key(d,"Hands","Kick") || Key(d,"Hands","CombatRing") || Key(d,"Hands","EnemyAttackSpeed") ||
     Key(d,"Hands","Dismemberment")) return true;
 if (Key(d,"Hands","DismemberBlood") || Key(d,"Hands","DismemberCaps")) return c.hands.dismember.enabled;
 return false;
}
bool SettingShownIn(SettingsView view,const SettingDefinition& d,const Config& c) {
 if (view==SettingsView::All || view==SettingsView::Sections) return true;
 if (view==SettingsView::Combat) return ShownOnCombatPage(d,c);
 const bool snap=c.look.snapTurning;
 if (Key(d,"Look","SnapTurning")) return true;
 // The hand that holds the weapon: Full VR's.
 if (Key(d,"Hands","LeftHanded")) return c.fullVrMode;
 // Vanilla VR's own (Nadi, 2026-11-20): seated, the real eye height puts the
 // view low - the row says to turn it off then; the mouse's or gamepad's turn
 // eased while snap is off (VR View's, as Full VR's stick speed is Full VR's);
 // the laser in menus for controllers held as a gamepad; and sitting, falling
 // and mounting from the eyes, which some cannot stand. One page with the rest.
 if (Key(d,"Head","RealHeight") || Key(d,"Hands","ControllerMenus") || Key(d,"Look","FirstPersonAnimations"))
  return !c.fullVrMode;
 if (Key(d,"Look","SmoothTurning")) return !snap && !c.fullVrMode;
 // The comfort page ends the start guide: the switch for the guide is here
 // too, so it can be turned off where it ends (ui/FirstStart.h).
 if (Key(d,"Onboarding","StartGuide")) return true;
 if (Key(d,"Look","SnapTurnAngle") || Key(d,"Look","SnapTurnInstant") ||
     Key(d,"Look","SnapTurnVignette")) return snap;
 // Easing speed means nothing to an instant snap.
 if (Key(d,"Look","SnapTurnSpeed")) return snap && !c.look.snapTurnInstant;
 // Snap off in Full VR: the stick turns smoothly, and how fast is the
 // comfort question then (Nadi, 2026-10-21: "im onboarding auch eine option
 // anbieten beim smooth turn (also snap turn off) die geschwindigkeit zu
 // setzen") - the stick's own speed, Moving's row.
 if (Key(d,"Hands","TurnSpeed")) return !snap && c.fullVrMode;
 // How sneaking shows in Full VR, the vignette or the eye (vr/SneakIndicator.h;
 // Nadi, 2026-10-28: "im onboarding und settings sollen dann user entscheiden").
 if (Key(d,"Hands","SneakIndicator")) return c.fullVrMode;
 // The movement that most often makes one sick, and the steering every new
 // player feels at once (Nadi, 2026-11-14): where the stick walks, the
 // teleport's fade or glide and the glide's vignette, and no throw through
 // the air - the game's own, so in both modes.
 if (Key(d,"Hands","WalkDirection")) return c.fullVrMode;
 // Where the inventory is, in the hand or in front (vr/TabletLogic.h; Nadi,
 // 2026-11-17: the guide offers the traditional inventory, off).
 if (Key(d,"Tablet","Enabled")) return c.fullVrMode;
 // Where the character is on it (Nadi, 2026-11-18): only while the menus are in the hand.
 if (Key(d,"Tablet","Figure") || Key(d,"Tablet","Unroll")) return c.fullVrMode && c.hands.tablet.enabled;
 if (Key(d,"Locomotion","TeleportInstant")) return c.fullVrMode && c.hands.teleport.enabled;
 if (Key(d,"Locomotion","TeleportVignette"))
  return c.fullVrMode && c.hands.teleport.enabled && !c.hands.teleport.instant;
 if (Key(d,"Look","NoPlayerKnockdown")) return true;
 // The breath and the heavy weapon when tired (Nadi, 2026-11-15: "mittlerweile nervt mich das geatme"): taste, and
 // the breath grates on some - both while the exhaustion itself is on (off, the game's collapse comes instead).
 // Full VR's page only: Vanilla VR's stays at one page (2026-11-20); the settings have the row.
 if (Key(d,"Look","ExhaustionSounds")) return c.look.exhaustion.enabled && c.fullVrMode;
 if (Key(d,"Look","ExhaustionHeavyWeapon")) return c.look.exhaustion.enabled && c.fullVrMode;
 // The tips over the hand, which some find in the way: Full VR's.
 if (Key(d,"Onboarding","Hints")) return c.fullVrMode;
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
bool NativeSettings::OpenSections(SettingsMode mode,const Config& config) {
 m_mode=mode;
 if (!m_left.kept || m_left.mode!=mode) {
  SetView(SettingsView::Sections,config);
  return false;
 }
 // Sync moves the page and the row where an INI change since has left
 // fewer rows than they stood on.
 m_view=SettingsView::Sections; m_count=0;
 m_section=m_left.section; m_first=m_left.first; m_selected=m_left.selected;
 Sync(config);
 return true;
}
void NativeSettings::Leave() {
 if (m_view!=SettingsView::Sections) return;
 m_left.kept=true; m_left.mode=m_mode; m_left.section=m_section;
 m_left.first=m_first; m_left.selected=m_selected;
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
 // The comfort page keeps the start guide's order: the turn first, the hands,
 // how sneaking shows, and the guide's own switch last - the table is ordered
 // for the sections (2026-11-07) and no longer gives it.
 if (GuideView(m_view)) {
  const bool combat=m_view==SettingsView::Combat;
  const GuideRow* const order=combat ? kCombatOrder : kComfortOrder;
  const UInt32 n=combat ? sizeof(kCombatOrder)/sizeof(kCombatOrder[0]) : sizeof(kComfortOrder)/sizeof(kComfortOrder[0]);
  for (UInt32 k=0;k<n;++k) {
   const GuideRow& row=order[k];
   const SettingDefinition* d=FindSetting(row.section,row.key);
   if (!d || !SettingShownIn(m_view,*d,config) || m_count>=kNativeMaxRows) continue;
   const UInt32 i=static_cast<UInt32>(d-SettingDefinitions());
   if (i==m_selected) selectedShown=true;
   m_rows[m_count++]=i;
  }
 }
 for (UInt32 i=0;i<total && m_count<kNativeMaxRows && !GuideView(m_view);++i) {
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
 if (id==kNativeUndo) {
  float previous=0.0f;
  const auto* definition=UndoRow(previous);
  if (!definition) return r;
  // The row goes back to what it had; the selection follows it when the
  // view shows it, so the help names what was put back.
  if (!InOverview()) {
   for (UInt32 i=0;i<m_count;++i) {
    if (&SettingDefinitions()[m_rows[i]]==definition) {
     m_selected=m_rows[i];
     m_first=(i/kNativeSettingsRows)*kNativeSettingsRows;
     break;
    }
   }
  }
  r.definition=definition;
  r.value=previous;
  r.undo=true;
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
const SettingDefinition* NativeSettings::UndoRow(float& previous) const {
 UInt32 row=0;
 if (!m_undo.Top(row,previous) || row>=SettingDefinitionCount()) return nullptr;
 return &SettingDefinitions()[row];
}
void NativeSettings::NoteSaved(const NativeSettingEdit& edit,float previous) {
 if (!edit.definition || edit.action) return;
 if (edit.undo) { m_undo.Pop(); return; }
 m_undo.Push(static_cast<UInt32>(edit.definition-SettingDefinitions()),previous);
}
NativeEditResult CommitNativeEdit(const NativeSettingEdit& edit,Config& config,NativeSettingWriter& writer) {
 if (!edit.definition) return NativeEditResult::None;
 if (edit.action) {
  if (edit.definition->action==SettingAction::Recenter) writer.Recenter();
  if (edit.definition->action==SettingAction::AdjustHands) writer.AdjustHands();
  if (edit.definition->action==SettingAction::FitHolsters) writer.FitHolsters();
  if (edit.definition->action==SettingAction::PlaceStowSpot) writer.PlaceStowSpot();
  if (edit.definition->action==SettingAction::ShowControls) writer.ShowControls();
  if (edit.definition->action==SettingAction::SavePoseRecording) writer.SavePoseRecording();
  if (edit.definition->action==SettingAction::ShowHintsAgain) writer.ShowHintsAgain();
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
  // A line break: xOBSE's %r, expanded before the value is set (llde/xOBSE, read 2026-11-15) - the guide's pages
  // break their lines where they read well instead of wherever the wrap falls.
  else if (*p=='\n') b.Text("%r");
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
