#include "game/NativeMenuPrototype.h"
#include "game/MenuQueSignature.h"
#include "core/AddressSpace.h"
#include "core/AtomicFlag.h"
#include "core/Config.h"
#include "core/Log.h"
#include "game/GameAddresses.h"
#include "game/MenuType.h"
#include "game/MenuMode.h"
#include "game/PlayerAim.h"
#include "obse/NativeMenuApi.h"
#include "platform/PluginPath.h"
#include "platform/Win32Min.h"
#include "ui/NativeOnboarding.h"
#include "ui/NativeSettings.h"
#include "vr/HandInput.h"

namespace obvr::game {
namespace {
obse::ConsoleApi* g_console=nullptr;
bool (*g_poll)(int*)=nullptr;
ui::NativeOnboarding g_onboarding;
ui::NativeSettings g_settings;
bool g_checked=false, g_showOnboarding=false, g_saveFailed=false;
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
bool Run(const char* command) { return g_console->RunScriptLine2(command,nullptr,true); }
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
  GetConfig().handTracking=fullVR;
  GetConfig().hands.enabled=fullVR;
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
};

// Only changed text is sent through the script compiler. INI reloads appear
// on the next refresh without reopening the menu or resetting its selection.
char g_lastText[32][512]{};
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
 char page[16],pages[16],heading[96],temporary[64];
 ui::FormatInteger(static_cast<int>(g_settings.First()/ui::kNativeSettingsRows+1),page,sizeof(page));
 ui::FormatInteger(static_cast<int>(g_settings.Pages()),pages,sizeof(pages));
 Join(temporary,sizeof(temporary),page," / ",pages);
 Join(heading,sizeof(heading),"VR Settings (OBVR) - ",temporary);
 ok=CachedText(0,"user0",heading) && ok;
 for (UInt32 slot=0;slot<ui::kNativeSettingsRows;++slot) {
  char trait[96],label[192],value[32];
  const auto* definition=g_settings.Row(slot);
  ui::NativeRowTrait(slot,"visible",trait,sizeof(trait));
  ok=Number(trait,definition ? 1 : 0) && ok;
  if (!definition) continue;
  Join(label,sizeof(label),definition->category," / ",definition->label);
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
 ok=Number("parchment\\reset\\target",canReset ? 1 : 0) && ok;
 ok=Number("parchment\\reset\\alpha",canReset ? 255 : 110) && ok;
 const auto& selected=ui::SettingDefinitions()[g_settings.Selected()];
 char help[512];
 Join(help,sizeof(help),selected.label,": ",selected.help);
 ok=CachedText(23,"user1",g_refusal ? g_refusal : g_saveFailed ? "Could not save OBVR.ini. The setting was not changed. Check file permissions and try again." : help) && ok;
 return ok;
}
void FinishSettings() {
 g_settingsOpen.Set(false); g_ourRoot=nullptr;
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
  const auto edit=g_settings.Click(button,GetConfig());
  SettingWriter writer;
  const auto result=ui::CommitNativeEdit(edit,GetConfig(),writer);
  if (result!=ui::NativeEditResult::None) {
   g_saveFailed=result==ui::NativeEditResult::SaveFailed;
   g_refusal=result==ui::NativeEditResult::Refused
    ? ui::SettingEditRefusal(*edit.definition,GetConfig(),edit.value) : nullptr;
  }
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
 if (ui::SettingsStep(false,GenericRoot()!=nullptr,false,false,toggle,false,false,
     top==kMenuIdLoading,top!=0 || PlayerInWorld())==ui::NativeSettingsStep::Open) {
  if (OpenMenu(true)) {
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
