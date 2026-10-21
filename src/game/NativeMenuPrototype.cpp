#include "game/NativeMenuPrototype.h"
#include <cstring>
#include "game/MenuQueSignature.h"
#include "core/AddressSpace.h"
#include "core/AtomicFlag.h"
#include "core/Config.h"
#include "core/Log.h"
#include "game/GameAddresses.h"
#include "game/HandAdjust.h"
#include "game/MenuType.h"
#include "game/MenuMode.h"
#include "game/PlayerAim.h"
#include "obse/NativeMenuApi.h"
#include "platform/PluginPath.h"
#include "platform/Win32Min.h"
#include "ui/NativeOnboarding.h"
#include "ui/NativeHandAdjust.h"
#include "vr/StowPlace.h"
#include "ui/NativeSettings.h"
#include "core/UpdateNotice.h"
#include "ui/NativeControlsCard.h"
#include "ui/FirstStart.h"
#include "test/HandScriptRuntime.h"
#include "platform/UpdateFetch.h"
#include "vr/HandInput.h"

namespace obvr::game {
namespace {
obse::ConsoleApi* g_console=nullptr;
bool (*g_poll)(int*)=nullptr;
ui::NativeOnboarding g_onboarding;
ui::NativeSettings g_settings;
bool g_checked=false, g_showOnboarding=false, g_saveFailed=false;
// Full VR was just chosen: the comfort page follows once the choice has closed.
bool g_comfortPending=false;
// "Adjust hands" was chosen: the settings menu closes, the guide opens after.
bool g_closeForAdjust=false;
// The controls card (ui/NativeControlsCard.h) is up.
bool g_cardOpen=false;
// The first start's chain (ui/FirstStart.h): which page, whether its window
// is up, whether the recenter has fired, which page of the card.
struct FirstStartChain {
 bool active=false;
 bool windowUp=false;
 bool recentred=false;
 ui::FirstStartPage page=ui::FirstStartPage::Recenter;
 ui::ControlsCardPage cardPage=ui::kControlsCardFirstPage;
};
FirstStartChain g_chain;
// The chain ran in the world (a hand script's ask): its comfort page opens
// there, not only over the main menu.
bool g_comfortInWorld=false;
const char* g_refusal=nullptr; // why the last change was refused, shown in the help line
void* g_ourRoot=nullptr;
AtomicFlag g_available, g_suppressLegacy, g_settingsOpen, g_toggleRequested, g_recenterRequested;
vr::ButtonEdge g_insertEdge, g_escapeEdge, g_previousEdge, g_nextEdge;
UInt32 g_refreshTicks=0;

void* GenericRoot() {
 const UInt16 count=*reinterpret_cast<const UInt16*>(addr::kTileMenuArrayCount);
 const auto data=*reinterpret_cast<void***>(addr::kTileMenuArrayData);
 const UInt32 index=kMenuIdGeneric-kMenuIdFirst;
 if (!mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(data)) || count<=index) return nullptr;
 return data[index];
}
UInt32 TopMenu() {
 void* manager=*reinterpret_cast<void**>(addr::kInterfaceManagerPointer);
 if (!mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(manager))) return 0;
 using Fn=UInt32(__fastcall*)(void*,void*);
 return reinterpret_cast<Fn>(addr::kGetTopVisibleMenuId)(manager,nullptr);
}
bool Run(const char* command) {
 const bool ran=g_console->RunScriptLine2(command,nullptr,true);
 // Which line, the first several times: "a UI update command failed" alone
 // left the semicolon to be found by elimination (2026-09-25).
 static UInt32 s_failLinesLeft=8;
 if (!ran && s_failLinesLeft>0) { --s_failLinesLeft; OBVR_LOG("Native menus: script line failed: %s",command); }
 return ran;
}
bool Text(const char* trait,const char* text) {
 char command[1024];
 return ui::NativeTextCommand(trait,text,command,sizeof(command)) && Run(command);
}
bool Number(const char* trait,int value) {
 char command[192];
 return ui::NativeFloatCommand(trait,value,command,sizeof(command)) && Run(command);
}
bool AssetExists(const char* name) {
 char path[512];
 return platform::BuildGamePath(name,path,sizeof(path)) && GetFileAttributesA(path)!=0xFFFFFFFFu;
}
bool CheckBackend() {
 auto module=GetModuleHandleA("Submodule.Game.dll");
 if (!module) { OBVR_LOG("Native menus: MenuQue missing; legacy menus retained"); return false; }
 const auto base=reinterpret_cast<UInt32>(module);
 const auto bytes=reinterpret_cast<const UInt8*>(module);
 const UInt32 pe=*reinterpret_cast<const UInt32*>(bytes+0x3C);
 if (bytes[0]!='M' || bytes[1]!='Z' || pe>4096 ||
     *reinterpret_cast<const UInt32*>(bytes+pe)!=0x4550 ||
     *reinterpret_cast<const UInt32*>(bytes+pe+24+56)<0x50EDC ||
     !MenuQuePollMatches(bytes+0xEA30,base)) {
  OBVR_LOG("Native menus: unsupported MenuQue binary; legacy menus retained"); return false;
 }
 if (!AssetExists("Data\\Menus\\Generic\\OBVR_Onboarding.xml") ||
     !AssetExists("Data\\Menus\\Generic\\OBVR_Settings.xml") ||
     !AssetExists("Data\\Menus\\Prefabs\\OBVR\\button_highlight.xml")) {
  OBVR_LOG("Native menus: XML assets missing; legacy menus retained"); return false;
 }
 g_poll=reinterpret_cast<bool (*)(int*)>(base+0xEA30);
 OBVR_LOG("Native menus: MenuQue v16b poll verified, native onboarding and settings ready");
 return true;
}
bool OpenMenu(bool settings) {
 if (GenericRoot()) return false;
 int stale=-1; g_poll(&stale);
 const bool executed=Run(settings ? "ShowGenericMenu \"OBVR_Settings.xml\" 9299"
                                  : "ShowGenericMenu \"OBVR_Onboarding.xml\"");
 g_ourRoot=GenericRoot();
 OBVR_LOG("Native menus: open %s executed=%d root=%08X",settings ? "settings" : "onboarding",executed,reinterpret_cast<UInt32>(g_ourRoot));
 return executed && g_ourRoot;
}
class OnboardingPort final: public ui::NativeMenuPort {
 bool Open(ui::NativePage page) override {
  if (!OpenMenu(false)) return false;
  if (page==ui::NativePage::SaveFailed)
   Text("user0","Could not save your choice. Check that OBVR.ini is writable and try again.");
  return true;
 }
 bool SaveMode(bool fullVR) override {
  if (!SaveSetting("Hands","Enabled",fullVR ? "1" : "0")) return false;
  // Chosen once: the walkthrough does not come back at the next start. It
  // can be switched on again in the settings (Onboarding / Show at start).
  if (SaveSetting("Onboarding","ShowAtStart","0")) GetConfig().onboardingShowAtStart=false;
  GetConfig().fullVrMode=fullVR;
  GetConfig().hands.enabled=fullVR;
  // The comfort page follows - as the start guide's last page while the
  // guide is on (ui/FirstStart.h), by itself otherwise.
  g_comfortPending=fullVR && !GetConfig().onboardingStartGuide;
  OBVR_LOG("Native onboarding: %s selected and saved",fullVR ? "Full VR" : "Keyboard/Gamepad + VR");
  return true;
 }
};
class SettingWriter final: public ui::NativeSettingWriter {
 bool Save(const ui::SettingDefinition& definition,const char* value) override {
  const bool saved=SaveSetting(definition.iniSection,definition.iniKey,value);
  OBVR_LOG("Native settings: %s %s.%s=%s",saved ? "saved" : "could not save",definition.iniSection,definition.iniKey,value);
  return saved;
 }
 void Recenter() override { g_recenterRequested.Set(true); }
 void AdjustHands() override { game::RequestHandAdjustGuide(); g_closeForAdjust=true; }
 void FitHolsters() override {
  game::RequestHolsterFit();
  g_closeForAdjust=true;
  OBVR_LOG("Native settings: Fit weapon places - the menu closes and the fit starts");
 }
 void PlaceStowSpot() override {
  game::RequestStowPlaceWindow();
  g_closeForAdjust=true;
  OBVR_LOG("Native settings: Place the stow spot - the menu closes and its window opens");
 }
 void ShowControls() override {
  game::RequestControlsCard();
  g_closeForAdjust=true;
  OBVR_LOG("Native settings: Show the controls - the menu closes and the card opens");
 }
};

// Only changed text is sent through the script compiler. INI reloads appear
// on the next refresh without reopening the menu or resetting its selection.
char g_lastText[48][512]{};
void InvalidateText() { for (auto& row:g_lastText) row[0]='\0'; }
bool SameText(const char* a,const char* b) { while (*a && *a==*b) { ++a; ++b; } return *a==*b; }
bool CachedText(UInt32 slot,const char* trait,const char* text) {
 if (SameText(g_lastText[slot],text)) return true;
 if (!Text(trait,text)) return false;
 UInt32 i=0;
 for (;text[i] && i+1<sizeof(g_lastText[slot]);++i) g_lastText[slot][i]=text[i];
 g_lastText[slot][i]='\0';
 return true;
}
void Join(char* out,UInt32 capacity,const char* a,const char* b,const char* c="") {
 UInt32 at=0;
 const char* parts[]={a,b,c};
 for (const char* part:parts) for (;*part && at+1<capacity;++part) out[at++]=*part;
 out[at]='\0';
}
bool RefreshSettings() {
 bool ok=true;
 // An edit or an INI reload may have turned a row that others follow - or
 // switched Full VR on or off, which brings the other mode's menu.
 const auto mode=GetConfig().fullVrMode ? ui::SettingsMode::FullVr : ui::SettingsMode::VrView;
 if (g_settings.View()==ui::SettingsView::Sections && g_settings.Mode()!=mode)
  g_settings.SetMode(mode,GetConfig());
 g_settings.Sync(GetConfig());
 const bool comfort=g_settings.View()==ui::SettingsView::Comfort;
 const bool overview=g_settings.InOverview();
 const char* const section=g_settings.OpenSection();
 char page[16],pages[16],heading[96],temporary[64],title[96];
 ui::FormatInteger(static_cast<int>(g_settings.First()/ui::kNativeSettingsRows+1),page,sizeof(page));
 ui::FormatInteger(static_cast<int>(g_settings.Pages()),pages,sizeof(pages));
 Join(temporary,sizeof(temporary),page," / ",pages);
 if (comfort) Join(title,sizeof(title),"Comfort (OBVR)","","");
 else if (section) Join(title,sizeof(title),ui::SettingsModeName(g_settings.Mode()),": ",section);
 else Join(title,sizeof(title),ui::SettingsModeName(g_settings.Mode())," (OBVR)","");
 Join(heading,sizeof(heading),title," - ",temporary);
 ok=CachedText(0,"user0",heading) && ok;
 // Back from a section to the list of sections; hidden elsewhere, the same
 // way as the paging buttons (alpha and target, XML booleans).
 const bool back=section!=nullptr;
 ok=Number("parchment\\back\\alpha",back ? 255 : 0) && ok;
 ok=Number("parchment\\back\\target",ui::kXmlBool[back ? 1 : 0]) && ok;
 ok=CachedText(42,"parchment\\back\\label\\string",back ? "Back" : " ") && ok;
 ok=CachedText(24,"parchment\\close\\label\\string",comfort ? "Continue" : "Close") && ok;
 // One page has nowhere to turn to: no Previous, no Next. Oblivion's XML
 // booleans are &true; = 2 and &false; = 1, any other number false (UESP,
 // Oblivion XML/Entities) - a target written as 1 switched both buttons
 // off in the full menu (2026-09-25). Hidden by alpha, which the reset
 // button already dims by; visible = 0 left them drawn.
 const bool paging=ui::NativePagingShown(g_settings.Pages());
 const char* const pagers[]={"parchment\\previous\\","parchment\\next\\"};
 for (const char* button:pagers) {
  char trait[64];
  Join(trait,sizeof(trait),button,"alpha");
  ok=Number(trait,paging ? 255 : 0) && ok;
  Join(trait,sizeof(trait),button,"target");
  ok=Number(trait,ui::kXmlBool[paging ? 1 : 0]) && ok;
 }
 // And the words themselves, in case the alpha does not reach the label.
 ok=CachedText(40,"parchment\\previous\\label\\string",paging ? "Previous" : " ") && ok;
 ok=CachedText(41,"parchment\\next\\label\\string",paging ? "Next" : " ") && ok;
 for (UInt32 slot=0;slot<ui::kNativeSettingsRows;++slot) {
  char trait[96],label[192],value[32];
  const auto* definition=g_settings.Row(slot);
  // The list of sections: each row a section, its size as the value, the
  // whole row a button that opens it; no - or +.
  if (overview) {
   const char* name=g_settings.SectionAt(slot);
   const char* const rowParts[]={"","minus\\","plus\\"};
   for (const char* part:rowParts) {
    char suffix[32];
    Join(suffix,sizeof(suffix),part,"target");
    ui::NativeRowTrait(slot,suffix,trait,sizeof(trait));
    ok=Number(trait,ui::kXmlBool[name && part[0]=='\0' ? 1 : 0]) && ok;
   }
   ui::NativeRowTrait(slot,"minus\\label\\string",trait,sizeof(trait));
   ok=CachedText(25+slot*2,trait," ") && ok;
   ui::NativeRowTrait(slot,"plus\\label\\string",trait,sizeof(trait));
   ok=CachedText(26+slot*2,trait,name ? ">" : " ") && ok;
   ui::NativeRowTrait(slot,"label\\string",trait,sizeof(trait));
   ok=CachedText(1+slot*3,trait,name ? name : " ") && ok;
   // No row count beside a section: it was noise (the tester, 2026-09-28).
   ui::NativeRowTrait(slot,"value\\string",trait,sizeof(trait));
   ok=CachedText(2+slot*3,trait," ") && ok;
   ui::NativeRowTrait(slot,"restart\\string",trait,sizeof(trait));
   ok=CachedText(3+slot*3,trait," ") && ok;
   continue;
  }
  // A slot past the last row is emptied rather than hidden: visible was
  // written as 1/0, which are both false to the XML (&true; is 2), and the
  // rows stayed drawn - with the text of whatever stood there before, so a
  // comfort page that lost a row showed "Left-handed" twice (2026-09-25).
  // Blank text, and not a target, is what an empty slot is made of.
  const char* const parts[]={"","minus\\","plus\\"};
  for (const char* part:parts) {
   char suffix[32];
   Join(suffix,sizeof(suffix),part,"target");
   ui::NativeRowTrait(slot,suffix,trait,sizeof(trait));
   ok=Number(trait,ui::kXmlBool[definition ? 1 : 0]) && ok;
  }
  ui::NativeRowTrait(slot,"minus\\label\\string",trait,sizeof(trait));
  ok=CachedText(25+slot*2,trait,definition ? "-" : " ") && ok;
  ui::NativeRowTrait(slot,"plus\\label\\string",trait,sizeof(trait));
  ok=CachedText(26+slot*2,trait,definition ? "+" : " ") && ok;
  if (!definition) {
   ui::NativeRowTrait(slot,"label\\string",trait,sizeof(trait));
   ok=CachedText(1+slot*3,trait," ") && ok;
   ui::NativeRowTrait(slot,"value\\string",trait,sizeof(trait));
   ok=CachedText(2+slot*3,trait," ") && ok;
   ui::NativeRowTrait(slot,"restart\\string",trait,sizeof(trait));
   ok=CachedText(3+slot*3,trait," ") && ok;
   continue;
  }
  // In a section its name is the heading; in the other views it leads.
  if (section) Join(label,sizeof(label),definition->label,"","");
  else Join(label,sizeof(label),definition->category," / ",definition->label);
  const auto item=ui::ItemFor(*definition,GetConfig());
  ui::FormatValue(item,value,sizeof(value));
  ui::NativeRowTrait(slot,"label\\string",trait,sizeof(trait));
  ok=CachedText(1+slot*3,trait,label) && ok;
  ui::NativeRowTrait(slot,"value\\string",trait,sizeof(trait));
  ok=CachedText(2+slot*3,trait,value) && ok;
  ui::NativeRowTrait(slot,"restart\\string",trait,sizeof(trait));
  ok=CachedText(3+slot*3,trait,definition->needsRestart ? "restart required" : " ") && ok;
 }
 const bool canReset=g_settings.CanResetSelected(GetConfig());
 ok=Number("parchment\\reset\\target",ui::kXmlBool[canReset ? 1 : 0]) && ok;
 ok=Number("parchment\\reset\\alpha",canReset ? 255 : 110) && ok;
 // Undo: lit while a change is left to put back, its label naming the row
 // and the value it goes back to.
 float previous=0.0f;
 const auto* undoRow=g_settings.UndoRow(previous);
 char undoLabel[160];
 if (undoRow) {
  auto item=ui::ItemFor(*undoRow,GetConfig());
  item.value=previous;
  char value[64];
  ui::FormatValue(item,value,sizeof(value));
  Join(undoLabel,sizeof(undoLabel),"Undo: ",undoRow->label," = ");
  const UInt32 used=static_cast<UInt32>(std::strlen(undoLabel));
  Join(undoLabel+used,sizeof(undoLabel)-used,value,"","");
 } else {
  Join(undoLabel,sizeof(undoLabel),"Undo last change","","");
 }
 ok=Number("parchment\\undo\\target",ui::kXmlBool[undoRow ? 1 : 0]) && ok;
 ok=Number("parchment\\undo\\alpha",undoRow ? 255 : 110) && ok;
 ok=CachedText(43,"parchment\\undo\\label\\string",undoLabel) && ok;
 const auto& selected=ui::SettingDefinitions()[g_settings.Selected()];
 char help[512];
 if (overview) Join(help,sizeof(help),"Select a section to see its settings. Back returns here.","","");
 else Join(help,sizeof(help),selected.label,": ",selected.help);
 ok=CachedText(23,"user1",g_refusal ? g_refusal : g_saveFailed ? "Could not save OBVR.ini. The setting was not changed. Check file permissions and try again." : help) && ok;
 return ok;
}
// The controls card's page onto its window (ui::ControlsCardFor): the title,
// the five lines, the page's drawing shown and the others hidden, the two
// button labels.
bool FillControlsCard(ui::ControlsCardPage page) {
 const ui::ControlsCardText text=ui::ControlsCardFor(page,GetConfig().hands.leftHanded);
 bool ok=Text("user0",text.title);
 for (UInt32 i=0;i<ui::kControlsCardLines;++i) {
  char trait[32];
  ok=ui::ControlsCardLineTrait(i,trait,sizeof(trait)) && Text(trait,text.lines[i]) && ok;
 }
 for (UInt32 i=0;i<ui::kControlsCardDrawings;++i) {
  char trait[32];
  ok=ui::ControlsCardDrawingTrait(i,trait,sizeof(trait)) && Number(trait,i==text.drawing ? 255 : 0) && ok;
 }
 ok=Text("user2",text.primary) && ok;
 ok=Text("user3",text.secondary) && ok;
 OBVR_LOG("Native menus: controls card - %s, drawing %u%s",text.title,text.drawing,ok ? "" : " (a UI update command failed)");
 return ok;
}
// A first-start page's words onto the adjust-hands window
// (ui::FirstStartTextFor): the title, the text, the three button labels, the
// third button shown only where it is a choice.
bool FillFirstStart(ui::FirstStartPage page,bool recentred) {
 const ui::FirstStartText t=ui::FirstStartTextFor(page,recentred);
 bool ok=Text("user0",t.title);
 ok=Text("user1",t.message) && ok;
 ok=Text("user2",t.primary) && ok;
 ok=Text("user3",t.secondary) && ok;
 ok=Text("user4",t.reset) && ok;
 ok=Number("parchment\\reset\\target",ui::kXmlBool[t.resetShown ? 1 : 0]) && ok;
 ok=Number("parchment\\reset\\alpha",t.resetShown ? 255 : 0) && ok;
 OBVR_LOG("Native menus: first start - %s%s",t.title,ok ? "" : " (a UI update command failed)");
 return ok;
}
void FinishSettings() {
 g_settingsOpen.Set(false); g_ourRoot=nullptr;
 // The comfort page is a one-off; Insert opens the whole menu again.
 if (g_settings.View()!=ui::SettingsView::Sections) g_settings.SetView(ui::SettingsView::Sections,GetConfig());
 OBVR_LOG("Native settings: closed");
}
void CloseSettings() {
 // Click OUR close tile, never CloseAllMenus or a foreign generic menu.
 if (g_ourRoot && GenericRoot()==g_ourRoot) Run("ClickMenuButton \"parchment\\close\" 1011");
 if (!GenericRoot() || GenericRoot()!=g_ourRoot) FinishSettings();
}
bool Down(UInt32 key) { return key && (GetAsyncKeyState(static_cast<int>(key))&0x8000)!=0; }
void Tick() {
 if (!g_checked) {
  g_checked=true;
  const bool available=CheckBackend();
  g_available.Set(available); g_suppressLegacy.Set(available);
 }
 if (!g_available.Get()) return;
 const UInt32 top=TopMenu();
 // A hand script's doors (test/HandScript.h "action"): "controls-card" asks
 // for the card, "first-start" begins the chain in the world - the script
 // itself runs only in the world with no menu up, so neither may open by
 // itself before it (controls-card.txt, first-start.txt).
 static bool s_chainInWorld=false;
 if (test::TakeHandScriptAction("controls-card")) {
  game::RequestControlsCard();
  OBVR_LOG("Native menus: a hand script asks for the controls card");
 }
 if (test::TakeHandScriptAction("first-start")) {
  s_chainInWorld=true;
  OBVR_LOG("Native menus: a hand script asks for the first start's guide in the world");
 }
 const bool toggleKey=vr::StepRisingEdge(g_insertEdge,Down(GetConfig().settingsMenuKey));
 const bool toggle=g_toggleRequested.Take() || toggleKey;
 const bool escape=vr::StepRisingEdge(g_escapeEdge,Down(0x1B));
 const bool previous=vr::StepRisingEdge(g_previousEdge,Down(0x21));
 const bool next=vr::StepRisingEdge(g_nextEdge,Down(0x22));
 void* root=GenericRoot();
 const bool foreign=root && root!=g_ourRoot;
 if (g_settingsOpen.Get()) {
  int button=-1;
  if (!foreign && (top==kMenuIdGeneric || !root) && !g_poll(&button)) button=-1;
  const auto step=ui::SettingsStep(true,root!=nullptr,foreign,top==kMenuIdGeneric,
   toggle,escape,button==ui::kNativeClose,false,false);
  if (step==ui::NativeSettingsStep::Release) { FinishSettings(); return; }
  if (step==ui::NativeSettingsStep::Wait) return;
  if (step==ui::NativeSettingsStep::Close) { CloseSettings(); return; }
  if (previous) button=ui::kNativePrevious;
  if (next) button=ui::kNativeNext;
  // Which button a click reached and which row stood there: "I switched the
  // dot off and the beam went" (2026-09-25) had only the saved key to go by.
  if (button>=ui::kNativeRowBase && button<ui::kNativeRowBase+static_cast<int>(ui::kNativeSettingsRows)*3) {
   const auto* row=g_settings.Row(static_cast<UInt32>(button-ui::kNativeRowBase)/3);
   static const char* const kParts[]={"label","-","+"};
   OBVR_LOG("Native settings: clicked %s on row %d (%s)",kParts[(button-ui::kNativeRowBase)%3],
            (button-ui::kNativeRowBase)/3,row ? row->label : "empty");
  }
  const auto edit=g_settings.Click(button,GetConfig());
  // What the row had before the change, for the Undo button's queue.
  const float before=edit.definition && !edit.action ? ui::ItemFor(*edit.definition,GetConfig()).value : 0.0f;
  SettingWriter writer;
  const auto result=ui::CommitNativeEdit(edit,GetConfig(),writer);
  if (result==ui::NativeEditResult::Saved) {
   g_settings.NoteSaved(edit,before);
   if (edit.undo) OBVR_LOG("Native settings: undo put %s back to %g",edit.definition->label,static_cast<double>(edit.value));
  }
  if (result!=ui::NativeEditResult::None) {
   g_saveFailed=result==ui::NativeEditResult::SaveFailed;
   g_refusal=result==ui::NativeEditResult::Refused
    ? ui::SettingEditRefusal(*edit.definition,GetConfig(),edit.value) : nullptr;
  }
  if (g_closeForAdjust) { g_closeForAdjust=false; CloseSettings(); return; }
  if (edit.repaint || ++g_refreshTicks>=30) {
   g_refreshTicks=0;
   if (!RefreshSettings()) OBVR_LOG("Native settings: a UI update command failed");
  }
  return;
 }
 if (g_showOnboarding && g_onboarding.State()!=ui::NativeState::Done && g_onboarding.State()!=ui::NativeState::Failed) {
  int button=-1;
  if (g_onboarding.State()==ui::NativeState::Open && !foreign && !g_poll(&button)) button=-1;
  OnboardingPort port;
  g_onboarding.Tick(top==kMenuIdMain,foreign,button,port);
  if (g_onboarding.State()==ui::NativeState::Failed) g_suppressLegacy.Set(false);
  if (g_onboarding.State()==ui::NativeState::Done) g_ourRoot=nullptr;
  if (g_onboarding.State()==ui::NativeState::Open) return;
 }
 // The first start's chain (ui/FirstStart.h): Recenter, Which hand, the
 // controls card, then the comfort page - at every start with Full VR and
 // [Onboarding] StartGuide on, in the main menu (a hand script's "action
 // first-start": once in the world, the harness's door). Each page can
 // be skipped, Esc ends the chain; a hand script presses the buttons with
 // "action menu-primary", "menu-secondary" and "menu-reset".
 {
  static bool s_ran=false;
  // The chain's place: the main menu, or the world for a hand script's run -
  // asked for, or already running there.
  const bool placeInFront=(s_chainInWorld || (g_chain.active && g_comfortInWorld)) ? (top==0 && PlayerInWorld())
                                                                              : top==kMenuIdMain;
  const bool tileBusy=g_showOnboarding && g_onboarding.State()!=ui::NativeState::Done &&
   g_onboarding.State()!=ui::NativeState::Failed;
  // A hand script's ask counts as the guide on and not yet run (its run's
  // INI keeps the guide off, so the chain cannot stand in front of the main
  // menu's console that loads the save).
  if (!g_chain.active && ui::FirstStartDue(GetConfig().fullVrMode,GetConfig().onboardingStartGuide || s_chainInWorld,
      placeInFront,root!=nullptr,g_comfortPending || g_settingsOpen.Get() || g_cardOpen || tileBusy,
      s_ran && !s_chainInWorld)) {
   s_ran=true;
   g_comfortInWorld=s_chainInWorld;  // the comfort page opens where the chain ran
   s_chainInWorld=false;
   g_chain=FirstStartChain{};
   g_chain.active=true;
   OBVR_LOG("Native menus: the first start's guide begins");
  }
  if (g_chain.active) {
   if (g_chain.page==ui::FirstStartPage::Comfort) {
    g_chain.active=false;
    g_comfortPending=true;
    OBVR_LOG("Native menus: the first start's guide ends with the comfort page");
   } else if (!g_chain.windowUp) {
    if (root) return;  // the page before is still closing
    if (!placeInFront) {
     g_chain.active=false;
     OBVR_LOG("Native menus: the first start's guide stops - its place is no longer in front");
     return;
    }
    const char* const xml=ui::FirstStartWindow(g_chain.page);
    if (!xml || !AssetExists(xml)) {
     OBVR_LOG("Native menus: first start - %s missing, the page is skipped",xml ? xml : "the window");
     g_chain.page=ui::FirstStartNext(g_chain.page);
     return;
    }
    int stale=-1; g_poll(&stale);
    const bool card=g_chain.page==ui::FirstStartPage::Controls;
    const bool executed=Run(card ? "ShowGenericMenu \"OBVR_Controls.xml\" 9600"
                                 : "ShowGenericMenu \"OBVR_AdjustHands.xml\" 9500");
    g_chain.windowUp=executed && GenericRoot();
    if (g_chain.windowUp) {
     g_ourRoot=GenericRoot();
     if (card) {
      g_chain.cardPage=ui::kControlsCardFirstPage;
      FillControlsCard(g_chain.cardPage);
     } else {
      if (g_chain.page==ui::FirstStartPage::Recenter) game::TakeRecentredNote();  // an older recenter is not this page's
      FillFirstStart(g_chain.page,g_chain.recentred);
     }
    }
    OBVR_LOG("Native menus: first start - page open executed=%d",executed);
    return;
   } else {
    if (!root || foreign) {
     g_chain.active=false; g_chain.windowUp=false; g_ourRoot=nullptr;
     OBVR_LOG("Native menus: first start - the page was closed from outside, the guide ends");
     return;
    }
    if (g_chain.page==ui::FirstStartPage::Recenter && !g_chain.recentred && game::TakeRecentredNote()) {
     g_chain.recentred=true;
     FillFirstStart(g_chain.page,true);
    }
    int button=-1;
    if (top==kMenuIdGeneric && !g_poll(&button)) button=-1;
    const bool card=g_chain.page==ui::FirstStartPage::Controls;
    if (test::TakeHandScriptAction("menu-primary")) button=card ? ui::kControlsCardPrimary : ui::kAdjustHandsPrimary;
    if (test::TakeHandScriptAction("menu-secondary")) button=card ? ui::kControlsCardSecondary : ui::kAdjustHandsSecondary;
    if (test::TakeHandScriptAction("menu-reset")) button=ui::kAdjustHandsReset;
    const ui::FirstStartChoice choice=ui::ChooseFirstStart(g_chain.page,button);
    if (choice==ui::FirstStartChoice::None) return;
    if (choice==ui::FirstStartChoice::Turn) {
     g_chain.cardPage=ui::ControlsCardTurned(g_chain.cardPage);
     FillControlsCard(g_chain.cardPage);
     return;
    }
    if (choice==ui::FirstStartChoice::Right || choice==ui::FirstStartChoice::Left) {
     const bool left=choice==ui::FirstStartChoice::Left;
     const bool saved=SaveSetting("Hands","LeftHanded",left ? "1" : "0");
     if (saved) GetConfig().hands.leftHanded=left;
     OBVR_LOG("Native menus: first start - %s-handed %s",left ? "left" : "right",saved ? "saved" : "could not be saved");
    }
    OBVR_LOG("Native menus: first start - page done (button %d)",button);
    Run("ClickMenuButton \"parchment\\close\" 1011");
    g_chain.windowUp=false; g_ourRoot=nullptr;
    g_chain.page=ui::FirstStartNext(g_chain.page);
    return;
   }
  }
 }
 switch (ui::StepComfortPage(g_comfortPending,root!=nullptr,
                             top==kMenuIdMain || (g_comfortInWorld && top==0 && PlayerInWorld()))) {
 case ui::ComfortPageStep::None: break;
 case ui::ComfortPageStep::Wait: return;
 case ui::ComfortPageStep::Skip:
  g_comfortPending=false;
  OBVR_LOG("Native onboarding: comfort page skipped - the main menu is no longer in front");
  break;
 case ui::ComfortPageStep::Open:
  g_comfortPending=false;
  if (OpenMenu(true)) {
   g_settings.SetView(ui::SettingsView::Comfort,GetConfig());
   g_settingsOpen.Set(true); g_saveFailed=false; g_refusal=nullptr; InvalidateText();
   // Its rows by label, for the harness (the stick's turn speed with snap
   // off in Full VR since 2026-10-21, ui/NativeSettings.cpp).
   char rows[256]="";
   UInt32 at=0;
   for (UInt32 slot=0;slot<ui::kNativeSettingsRows;++slot) {
    const ui::SettingDefinition* d=g_settings.Row(slot);
    if (!d) continue;
    for (const char* p=at ? ", " : "";*p && at+1<sizeof(rows);++p) rows[at++]=*p;
    for (const char* p=d->label;*p && at+1<sizeof(rows);++p) rows[at++]=*p;
    rows[at]='\0';
   }
   OBVR_LOG("Native onboarding: comfort page open - rows: %s",rows);
   if (!RefreshSettings()) OBVR_LOG("Native settings: initial UI update failed");
  } else {
   OBVR_LOG("Native onboarding: the comfort page could not be opened");
  }
  return;
 }
 // The guided window for adjusting the hands (ui/NativeHandAdjust.h): the
 // guide when "Adjust hands" was chosen, the finish once both hands fit.
 {
  static bool s_open=false, s_guidePending=false, s_finishPending=false, s_assetMissing=false;
  static ui::HandAdjustPage s_page=ui::HandAdjustPage::Guide;
  if (game::TakeHandAdjustGuideRequest()) s_guidePending=true;
  if (game::TakeHandAdjustFinishRequest()) s_finishPending=true;
  // The first fit in Full VR, by itself, once per start until one is kept.
  // Hands already given their own grips (an older fit) count as fitted.
  static bool s_firstOffered=false, s_firstFit=false;
  {
   const vr::HandSettings& h=GetConfig().hands;
   const bool fitted=h.handsAdjusted || h.rightHandGripX!=0.0f || h.rightHandGripY!=0.0f ||
    h.rightHandGripZ!=0.0f || h.leftHandGripX!=0.0f || h.leftHandGripY!=0.0f || h.leftHandGripZ!=0.0f;
   if (ui::FirstHandFitDue(GetConfig().fullVrMode,fitted,PlayerInWorld() && top==0,s_firstOffered,
                           s_open || s_guidePending || s_finishPending || game::HandAdjustActive() ||
                           g_chain.active || g_cardOpen)) {
    s_firstOffered=true; s_firstFit=true; s_guidePending=true;
    OBVR_LOG("Native menus: the hands have never been fitted - the guide opens by itself");
   }
  }
  if (s_open) {
   if (!root || foreign) {
    // Closed from outside (Esc): the guide is a cancel, the finish a keep.
    s_open=false; g_ourRoot=nullptr;
    if (s_page==ui::HandAdjustPage::Finish) game::StopHandAdjust();
    OBVR_LOG("Native menus: adjust-hands window closed from outside");
    return;
   }
   int button=-1;
   if (top==kMenuIdGeneric && !g_poll(&button)) button=-1;
   const ui::HandAdjustChoice choice=ui::ChooseHandAdjust(s_page,button);
   if (choice==ui::HandAdjustChoice::None) return;
   switch (choice) {
   case ui::HandAdjustChoice::Start: game::StartHandAdjust(); break;
   case ui::HandAdjustChoice::Again: game::StartHandAdjust(); break;
   case ui::HandAdjustChoice::Keep: game::StopHandAdjust(); break;
   case ui::HandAdjustChoice::Reset: game::ResetHandsToDefaults(); game::StopHandAdjust(); break;
   case ui::HandAdjustChoice::Cancel: case ui::HandAdjustChoice::None: break;
   }
   if (ui::ChoiceSettlesFit(choice) && SaveSetting("Hands","HandsAdjusted","1")) GetConfig().hands.handsAdjusted=true;
   if (choice!=ui::HandAdjustChoice::Start && choice!=ui::HandAdjustChoice::Again) s_firstFit=false;
   OBVR_LOG("Native menus: adjust-hands window - button %d",button);
   s_open=false; g_ourRoot=nullptr;
   Run("ClickMenuButton \"parchment\\close\" 1011");
   return;
  }
  if ((s_guidePending || s_finishPending) && !root && top==0 && PlayerInWorld() && !s_assetMissing) {
   if (!AssetExists("Data\\Menus\\Generic\\OBVR_AdjustHands.xml")) {
    s_assetMissing=true;
    OBVR_LOG("Native menus: OBVR_AdjustHands.xml missing - adjusting starts without the guide");
    if (s_guidePending) game::StartHandAdjust();
    if (s_finishPending) game::StopHandAdjust();
    s_guidePending=s_finishPending=false;
    return;
   }
   s_page=s_finishPending ? ui::HandAdjustPage::Finish : ui::HandAdjustPage::Guide;
   s_guidePending=s_finishPending=false;
   int stale=-1; g_poll(&stale);
   const bool executed=Run("ShowGenericMenu \"OBVR_AdjustHands.xml\" 9500");
   s_open=executed && GenericRoot();
   if (s_open) g_ourRoot=GenericRoot();
   const bool finish=s_page==ui::HandAdjustPage::Finish;
   if (s_open) {
    Text("user0",finish ? "Your hands fit" : "Adjust your hands");
    Text("user1",finish
     ? "Both hands are fitted to your controllers. Keep this fit, adjust again, or put both hands back to their defaults."
     : "Your real controllers are shown where your hands are. Close a grip and that hand stays still. Move the controller into the hand the way you hold it, then open the grip. Do it for both hands. The grab is off meanwhile.");
    Text("user2",finish ? "Keep" : "Start");
    Text("user3",finish ? "Adjust again" : (s_firstFit ? "Later" : "Cancel"));
    Text("user4",finish ? "Reset" : " ");
    Number("parchment\\reset\\target",ui::kXmlBool[finish ? 1 : 0]);
    Number("parchment\\reset\\alpha",finish ? 255 : 0);
   }
   OBVR_LOG("Native menus: adjust-hands %s page - open executed=%d",finish ? "finish" : "guide",executed);
   return;
  }
 }
 // The window for placing the stow spot (vr/StowPlace.h), from the settings
 // row: the adjust-hands window with the stow's words. While it is open the
 // frame shows the ring and lets a grip move it; the buttons keep the spot,
 // put it back, or reset it - Esc puts it back.
 {
  static bool s_open=false, s_pending=false;
  if (game::TakeStowPlaceWindowRequest()) s_pending=true;
  if (s_open) {
   if (!root || foreign) {
    s_open=false; g_ourRoot=nullptr;
    game::SendStowPlaceCommand(static_cast<int>(vr::StowPlaceCommand::Cancel));
    OBVR_LOG("Native menus: stow-spot window closed from outside - the spot stays where it was");
    return;
   }
   int button=-1;
   if (top==kMenuIdGeneric && !g_poll(&button)) button=-1;
   vr::StowPlaceCommand command=vr::StowPlaceCommand::None;
   if (button==ui::kAdjustHandsPrimary) command=vr::StowPlaceCommand::Keep;
   else if (button==ui::kAdjustHandsSecondary) command=vr::StowPlaceCommand::Cancel;
   else if (button==ui::kAdjustHandsReset) command=vr::StowPlaceCommand::Reset;
   if (command==vr::StowPlaceCommand::None) return;
   game::SendStowPlaceCommand(static_cast<int>(command));
   OBVR_LOG("Native menus: stow-spot window - button %d",button);
   s_open=false; g_ourRoot=nullptr;
   Run("ClickMenuButton \"parchment\\close\" 1011");
   return;
  }
  if (s_pending && !root && top==0 && PlayerInWorld()) {
   s_pending=false;
   if (!AssetExists("Data\\Menus\\Generic\\OBVR_AdjustHands.xml")) {
    OBVR_LOG("Native menus: OBVR_AdjustHands.xml missing - the stow spot cannot be placed");
    return;
   }
   int stale=-1; g_poll(&stale);
   const bool executed=Run("ShowGenericMenu \"OBVR_AdjustHands.xml\" 9500");
   s_open=executed && GenericRoot();
   if (s_open) {
    g_ourRoot=GenericRoot();
    Text("user0","Place the stow spot");
    Text("user1","The gold ring shows where letting go of a held item puts it into your pack. Close a grip on the ring, move it where you want it on your body, and open the grip. Done keeps it there and hides the ring again.");
    Text("user2","Done");
    Text("user3","Cancel");
    Text("user4","Reset");
    Number("parchment\\reset\\target",ui::kXmlBool[1]);
    Number("parchment\\reset\\alpha",255);
    game::SendStowPlaceCommand(static_cast<int>(vr::StowPlaceCommand::Start));
   }
   OBVR_LOG("Native menus: stow-spot window - open executed=%d",executed);
   return;
  }
 }
 // The controls card (ui/NativeControlsCard.h): from the settings row "Show
 // the controls", the first start or a hand script's action - the Full VR
 // layout on two pages by role, in the main menu or in the world. Next and
 // Back turn the page, Close (or Esc, from outside) ends it.
 {
  static bool s_pending=false;
  static ui::ControlsCardPage s_page=ui::kControlsCardFirstPage;
  if (game::TakeControlsCardRequest()) s_pending=true;
  if (g_cardOpen) {
   if (!root || foreign) {
    g_cardOpen=false; g_ourRoot=nullptr;
    OBVR_LOG("Native menus: controls card closed from outside");
    return;
   }
   int button=-1;
   if (top==kMenuIdGeneric && !g_poll(&button)) button=-1;
   if (test::TakeHandScriptAction("menu-primary")) button=ui::kControlsCardPrimary;
   if (test::TakeHandScriptAction("menu-secondary")) button=ui::kControlsCardSecondary;
   const ui::ControlsCardChoice choice=ui::ChooseControlsCard(button);
   if (choice==ui::ControlsCardChoice::None) return;
   if (choice==ui::ControlsCardChoice::Close) {
    g_cardOpen=false; g_ourRoot=nullptr;
    Run("ClickMenuButton \"parchment\\close\" 1011");
    OBVR_LOG("Native menus: controls card closed");
    return;
   }
   s_page=ui::ControlsCardTurned(s_page);
   FillControlsCard(s_page);
   return;
  }
  if (s_pending && !root && (top==kMenuIdMain || (top==0 && PlayerInWorld()))) {
   s_pending=false;
   if (!AssetExists("Data\\Menus\\Generic\\OBVR_Controls.xml")) {
    OBVR_LOG("Native menus: OBVR_Controls.xml missing - no controls card");
    return;
   }
   int stale=-1; g_poll(&stale);
   const bool executed=Run("ShowGenericMenu \"OBVR_Controls.xml\" 9600");
   g_cardOpen=executed && GenericRoot();
   if (g_cardOpen) {
    g_ourRoot=GenericRoot();
    s_page=ui::kControlsCardFirstPage;
    FillControlsCard(s_page);
   }
   OBVR_LOG("Native menus: controls card - open executed=%d",executed);
   return;
  }
 }
 // The update notice (ui::StepUpdateWindow): a native window in the main

 // menu, the answer from GitHub or Debug.ForceUpdateNotice behind it.
 {
  static ui::UpdateWindowState s_update;
  static bool s_assetChecked=false, s_assetMissing=false;
  char tag[32]={};
  const bool newer=platform::LatestReleaseTag(tag,sizeof(tag)) &&
                   update::IsNewerVersion(tag,OBVR_VERSION_STRING);
  const bool known=(newer || GetConfig().forceUpdateNotice) && !s_assetMissing;
  const bool busy=g_comfortPending || g_settingsOpen.Get() || g_cardOpen || g_chain.active ||
   (g_showOnboarding && g_onboarding.State()!=ui::NativeState::Done &&
    g_onboarding.State()!=ui::NativeState::Failed);
  if (known && !s_assetChecked) {
   s_assetChecked=true;
   s_assetMissing=!AssetExists("Data\\Menus\\Generic\\OBVR_Update.xml");
   if (s_assetMissing) OBVR_LOG("Native menus: OBVR_Update.xml missing - no update notice");
  }
  const auto step=ui::StepUpdateWindow(s_update,known && !s_assetMissing,top==kMenuIdMain,
                                       root!=nullptr,busy);
  if (step==ui::UpdateWindowStep::Wait) return;
  if (step==ui::UpdateWindowStep::Open) {
   char line[96]={};
   update::FormatNotice(newer ? tag : OBVR_VERSION_STRING,line,sizeof(line));
   int stale=-1; g_poll(&stale);
   const bool executed=Run("ShowGenericMenu \"OBVR_Update.xml\" 9401");
   const bool texted=executed && GenericRoot() && Text("user0",line);
   OBVR_LOG("Native menus: update notice \"%s\" - open executed=%d text=%d",line,executed,texted);
   return;
  }
 }
 if (ui::SettingsStep(false,GenericRoot()!=nullptr,false,false,toggle,false,false,
     top==kMenuIdLoading,top!=0 || PlayerInWorld())==ui::NativeSettingsStep::Open) {
  if (OpenMenu(true)) {
   // Insert opens on the list of sections (ui::SettingsView::Sections) of
   // the menu for the mode that is on: VR View or the Full VR Port.
   g_settings.SetView(ui::SettingsView::Sections,GetConfig());
   g_settings.SetMode(GetConfig().fullVrMode ? ui::SettingsMode::FullVr : ui::SettingsMode::VrView,
                      GetConfig());
   g_settingsOpen.Set(true); g_saveFailed=false; g_refusal=nullptr; InvalidateText();
   if (!RefreshSettings()) OBVR_LOG("Native settings: initial UI update failed");
  } else if (!GenericRoot()) {
   // A failed ShowGenericMenu must not permanently swallow Insert.
   g_available.Set(false); g_suppressLegacy.Set(false);
   OBVR_LOG("Native settings: could not open XML; legacy menus restored");
  }
 }
}
}
void InstallNativeMenuPrototype(const obse::Interface* api) {
 // Retain NativePrototype as a compatibility switch for existing INIs.
 if (!GetConfig().nativeOnboardingPrototype) return;
 g_showOnboarding=GetConfig().onboardingShowAtStart;
 if (api->obseVersion<22 || !api->QueryInterface) return;
 g_console=static_cast<obse::ConsoleApi*>(api->QueryInterface(0));
 const auto tasks=static_cast<obse::TasksApi*>(api->QueryInterface(8));
 if (!g_console || g_console->version<2 || !g_console->RunScriptLine2 || !tasks || !tasks->EnqueueTask || !tasks->EnqueueTask(&Tick)) {
  OBVR_LOG("Native menus: xOBSE interfaces unavailable; legacy menus retained"); return;
 }
 g_suppressLegacy.Set(true);
}
bool NativeMenuSuppressesLegacy() { return g_suppressLegacy.Get(); }
bool NativeSettingsAvailable() { return g_available.Get(); }
bool NativeSettingsOpen() { return g_settingsOpen.Get(); }
void RequestNativeSettingsToggle() { g_toggleRequested.Set(true); }
bool TakeNativeRecenterRequest() { return g_recenterRequested.Take(); }
}
