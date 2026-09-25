#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "ui/NativeSettings.h"
#include "core/AtomicFlag.h"
using namespace obvr::ui;
using obvr::Config;
int failures=0;
void Check(bool b,const char* s) { if(!b) { ++failures; std::printf("FAIL %s\n",s); } }
struct Writer:NativeSettingWriter {
 bool success=true; int saves=0,recenters=0; char last[32]{};
 Config* live=nullptr; float expectedOld=0.0f; bool inspectOld=false; bool sawExpectedOld=false;
 bool Save(const SettingDefinition& definition,const char* s) override {
  ++saves;
  if (inspectOld && live != nullptr && definition.Read != nullptr &&
      definition.Read(*live) == expectedOld) {
   sawExpectedOld=true;
  }
  std::strcpy(last,s); return success;
 }
 void Recenter() override { ++recenters; }
};

float ChangedFromDefault(const SettingDefinition& definition, float canonical) {
 if (definition.kind == ItemKind::Toggle) {
  return canonical == 0.0f ? 1.0f : 0.0f;
 }
 return canonical != definition.maximum ? definition.maximum : definition.minimum;
}

void CheckSerializedCanonical(const SettingDefinition& definition, float canonical,
                              const Writer& writer) {
 if (definition.kind == ItemKind::Toggle && definition.falseWord[0] != '\0' &&
     definition.trueWord[0] != '\0') {
  const char* expected = canonical == 0.0f ? definition.falseWord : definition.trueWord;
  Check(std::strcmp(writer.last, expected) == 0, "reset writes the canonical word literal");
  return;
 }

 if (definition.kind == ItemKind::Toggle) {
  Check(std::strcmp(writer.last, canonical == 0.0f ? "0" : "1") == 0,
        "reset writes a numeric toggle literal");
  return;
 }

 char* end = nullptr;
 const float parsed = std::strtof(writer.last, &end);
 Check(end != writer.last && end != nullptr && *end == '\0' && parsed == canonical,
       "reset numeric text parses back to the canonical default");
}

void CheckOtherSettingsPreserved(const Config& before, const Config& after,
                                 const SettingDefinition& changed, const char* what) {
 const SettingDefinition* settings=SettingDefinitions();
 const UInt32 count=SettingDefinitionCount();
 bool preserved=true;
 for(UInt32 at=0;at<count;++at) {
  const SettingDefinition& definition=settings[at];
  if(&definition==&changed || definition.Read==nullptr) continue;
  if(definition.Read(before)!=definition.Read(after)) {
   preserved=false;
   std::printf("        reset of \"%s\" changed \"%s\"\n",changed.label,definition.label);
  }
 }
 Check(preserved,what);
}

void TestResetSelected() {
 std::printf("Selected reset proposals and transactional commits\n");
 NativeSettings menu;
 Writer writer;
 unsigned visited=0;
 unsigned editable=0;
 unsigned refusedAtDefault=0;

 for(unsigned page=0;page<menu.Pages();++page) {
  for(unsigned slot=0;slot<kNativeSettingsRows;++slot) {
   const auto* definition=menu.Row(slot);
   const int labelId=kNativeRowBase+static_cast<int>(slot)*3;
   if(!definition) {
    Config empty;
    const UInt32 selectedBefore=menu.Selected();
    Check(!menu.Click(labelId,empty).repaint && menu.Selected()==selectedBefore,
          "an empty page row cannot change selection");
    continue;
   }
   ++visited;

   Config config;
   float canonical=0.0f;
   Check(CanonicalDefaultValue(*definition,canonical),"every visible row has a canonical default");
   menu.Click(labelId,config);
   const UInt32 selectedBefore=menu.Selected();
   const int oldSaves=writer.saves;
   const int oldRecenters=writer.recenters;

   if(definition->kind==ItemKind::Action || definition->kind==ItemKind::Text) {
    Check(!menu.CanResetSelected(config),"actions and text rows disable reset");
    const auto proposal=menu.Click(kNativeReset,config);
    const int savesBeforeCommit=writer.saves;
    const int recentersBeforeCommit=writer.recenters;
    Check(proposal.definition==nullptr && menu.Selected()==selectedBefore &&
          CommitNativeEdit(proposal,config,writer)==NativeEditResult::None &&
          writer.saves==savesBeforeCommit && writer.recenters==recentersBeforeCommit &&
          writer.saves==oldSaves && writer.recenters==oldRecenters,
          "refused action reset changes no selection and performs no operation");
    continue;
   }

   ++editable;
   Check(!menu.CanResetSelected(config),"a value already at default disables reset");
   const auto atDefault=menu.Click(kNativeReset,config);
   const int savesBeforeDefault=writer.saves;
   const int recentersBeforeDefault=writer.recenters;
   Check(atDefault.definition==nullptr && menu.Selected()==selectedBefore,
         "reset at default is refused without changing selection");
   Check(CommitNativeEdit(atDefault,config,writer)==NativeEditResult::None &&
         writer.saves==savesBeforeDefault && writer.recenters==recentersBeforeDefault,
         "refused default reset performs no save or action");
   ++refusedAtDefault;

   const float changed=ChangedFromDefault(*definition,canonical);
   ApplySetting(*definition,config,changed);
   Check(ItemFor(*definition,config).value==changed,"test value differs from canonical default");
   Check(ItemFor(*definition,config).needsRestart==definition->needsRestart,
         "restart-required metadata remains attached to the selected row");
   const SettingDefinition* settings=SettingDefinitions();
   const UInt32 settingCount=SettingDefinitionCount();
   for(UInt32 other=0;other<settingCount;++other) {
    const SettingDefinition& otherDefinition=settings[other];
    if(&otherDefinition==definition || otherDefinition.kind==ItemKind::Action ||
       otherDefinition.kind==ItemKind::Text || otherDefinition.Write==nullptr) continue;
    float otherDefault=0.0f;
    if(CanonicalDefaultValue(otherDefinition,otherDefault)) {
     ApplySetting(otherDefinition,config,ChangedFromDefault(otherDefinition,otherDefault));
    }
   }
   const Config beforeFailure=config;
   Check(menu.CanResetSelected(config),"a changed editable value enables reset");
   const auto proposal=menu.Click(kNativeReset,config);
   Check(proposal.definition==definition && proposal.value==canonical &&
         menu.Selected()==selectedBefore && proposal.repaint,
         "reset proposes the selected row canonical value");

   writer.live=&config;
   writer.expectedOld=changed;
   writer.inspectOld=true;
   writer.sawExpectedOld=false;
   writer.success=false;
   Check(CommitNativeEdit(proposal,config,writer)==NativeEditResult::SaveFailed &&
         ItemFor(*definition,config).value==changed && writer.sawExpectedOld,
         "save failure preserves live value and observes it before apply");
   CheckOtherSettingsPreserved(beforeFailure,config,*definition,
                               "save failure preserves unrelated settings");
   Check(menu.Selected()==selectedBefore,"save failure preserves selection");

   writer.success=true;
   writer.sawExpectedOld=false;
   Check(CommitNativeEdit(proposal,config,writer)==NativeEditResult::Saved &&
         ItemFor(*definition,config).value==canonical && writer.sawExpectedOld,
         "retry saves before applying the canonical value");
   CheckOtherSettingsPreserved(beforeFailure,config,*definition,
                               "successful reset changes only the selected setting");
   CheckSerializedCanonical(*definition,canonical,writer);
   Check(!menu.CanResetSelected(config),"successful reset disables the default-valued row");
   const int savesAfter=writer.saves;
   const auto repeated=menu.Click(kNativeReset,config);
   const int recentersAfter=writer.recenters;
   Check(repeated.definition==nullptr &&
         CommitNativeEdit(repeated,config,writer)==NativeEditResult::None &&
         writer.saves==savesAfter && writer.recenters==recentersAfter &&
         menu.Selected()==selectedBefore,"repeating reset at default is inert");
  }
  Config pageConfig;
  menu.Click(kNativeNext,pageConfig);
 }

 Check(visited==SettingDefinitionCount(),"reset test visits every definition across pages");
 Check(editable+1==visited,"reset test includes the action refusal path");
 Check(refusedAtDefault==editable,"every editable definition refuses reset at default");
}

int main() {
 // Exhaust the lifecycle input combinations, including foreign/covered menus.
 for(unsigned mask=0;mask<512;++mask) {
  bool opened=mask&1,root=mask&2,foreign=mask&4,foreground=mask&8;
  bool toggle=mask&16,escape=mask&32,closed=mask&64,loading=mask&128,ready=mask&256;
  auto step=SettingsStep(opened,root,foreign,foreground,toggle,escape,closed,loading,ready);
  if(opened && (foreign || !root || closed)) Check(step==NativeSettingsStep::Release,"lost menu releases ownership");
  else if(opened && !foreground) Check(step==NativeSettingsStep::Wait,"covered menu ignores input");
  else if(opened) Check(step==((toggle||escape)?NativeSettingsStep::Close:NativeSettingsStep::Handle),"owned foreground handles input");
  else Check(step==((toggle && !root && !loading && ready)?NativeSettingsStep::Open:NativeSettingsStep::Wait),"opening eligibility");
 }
 obvr::AtomicFlag flag;
 Check(!flag.Get() && !flag.Take(),"empty signal"); flag.Set(true);
 Check(flag.Get() && flag.Get() && flag.Take() && !flag.Take(),"signal consumed once");
 flag.Set(true); flag.Set(false); Check(!flag.Get(),"signal clear");
 obvr::Config c; NativeSettings menu; Writer writer;
 Check(menu.First()==0 && menu.Selected()==0,"initial page");
 Check(!menu.Row(7) && !menu.Row(0xffffffff),"invalid row slots");
 Check(menu.Click(kNativeClose,c).close,"close");
 Check(!menu.Click(-1,c).repaint && !menu.Click(kNativeRowBase+21,c).repaint,"unknown IDs");
 menu.Click(kNativePrevious,c); Check(menu.First()==(menu.Pages()-1)*7,"previous wraps");
 menu.Click(kNativeNext,c); Check(menu.First()==0,"next wraps");
 unsigned visited=0;
 for(unsigned page=0;page<menu.Pages();++page) {
  for(unsigned slot=0;slot<7;++slot) {
   const auto* d=menu.Row(slot); int id=kNativeRowBase+slot*3;
   if(!d) { for(int part=0;part<3;++part) Check(!menu.Click(id+part,c).repaint,"unused row inert"); continue; }
   ++visited;
   char encoded[1024];
   Check(NativeTextCommand("user1",d->help,encoded,sizeof(encoded)),"every help text encodes");
   Check(NativeTextCommand("user0",d->label,encoded,sizeof(encoded)),"every label encodes");
   Check(menu.Click(id,c).repaint && menu.Selected()==menu.First()+slot,"label selects help");
   for(int part=1;part<=2;++part) {
    auto e=menu.Click(id+part,c);
    if(d->kind==ItemKind::Action) {
     int old=writer.recenters;
     Check(CommitNativeEdit(e,c,writer)==NativeEditResult::Action,"action result");
     Check(writer.recenters==old+(d->action==SettingAction::Recenter),"action dispatch"); continue;
    }
    const float before=ItemFor(*d,c).value;
    if(e.definition) {
     writer.success=false;
     Check(CommitNativeEdit(e,c,writer)==NativeEditResult::SaveFailed && ItemFor(*d,c).value==before,"failed save preserves live setting");
     writer.success=true;
     Check(CommitNativeEdit(e,c,writer)==NativeEditResult::Saved,"successful save applied");
     Check(ItemFor(*d,c).value==e.value,"live value follows persisted edit");
    } else Check(CommitNativeEdit(e,c,writer)==NativeEditResult::None,"unchanged edit inert");
   }
   if(d->kind==ItemKind::Number) {
    ApplySetting(*d,c,d->minimum); Check(!menu.Click(id+1,c).definition,"minimum clamp");
    ApplySetting(*d,c,d->maximum); Check(!menu.Click(id+2,c).definition,"maximum clamp");
   }
  }
  menu.Click(kNativeNext,c);
 }
 Check(visited==SettingDefinitionCount() && menu.First()==0,"every setting exposed once");
 menu.Click(kNativeNext,c); menu.Click(kNativePrevious,c); Check(menu.First()==0,"previous normal");
 SettingDefinition noAction; NativeSettingEdit action; action.definition=&noAction; action.action=true;
 Check(CommitNativeEdit(action,c,writer)==NativeEditResult::Action,"unknown action inert");
 char b[512];
 Check(NativeTextCommand("parchment\\row0\\label\\string","50% \"test\"",b,sizeof(b)) && std::strcmp(b,"SetMenuStringValue \"parchment\\row0\\label\\string@50%% %qtest%q\" 1011")==0,"console separator and format escaping");
 Check(!NativeTextCommand("user0","",b,sizeof(b)) && !*b,"empty operand refused: xOBSE would discard it");
 const char* bad[]={"@","|","\n","\r","\t"};
 for(auto s:bad) Check(!NativeTextCommand("user0",s,b,sizeof(b)) && !*b,"unsafe text refused");
 const char* traits[]={nullptr,"","bad\"","bad@","bad|","bad space","bad;"};
 for(auto t:traits) {
  Check(!NativeTextCommand(t,"x",b,sizeof(b)),"invalid text trait");
  Check(!NativeFloatCommand(t,1,b,sizeof(b)),"invalid numeric trait");
  Check(!NativeRowTrait(0,t,b,sizeof(b)),"invalid row suffix");
 }
 Check(!NativeTextCommand("user0",nullptr,b,sizeof(b)),"null text");
 Check(!NativeTextCommand("user0","x",nullptr,10) && !NativeTextCommand("user0","x",b,0),"invalid text buffer");
 Check(!NativeFloatCommand("user0",1,nullptr,10) && !NativeFloatCommand("user0",1,b,0),"invalid float buffer");
 Check(!NativeRowTrait(0,"visible",nullptr,10) && !NativeRowTrait(0,"visible",b,0),"invalid row buffer");
 Check(!NativeRowTrait(7,"visible",b,sizeof(b)),"invalid row command");
 for(unsigned i=0;i<7;++i) Check(NativeRowTrait(i,"value\\string",b,sizeof(b)) && b[13]=='0'+i,"row paths");
 Check(NativeFloatCommand("user0",-12,b,sizeof(b)) && std::strstr(b,"1011 -12"),"numeric command");
 for(int fn=0;fn<3;++fn) {
  auto build=[&](unsigned cap){return fn==0?NativeTextCommand("user0","x",b,cap):fn==1?NativeFloatCommand("user0",1,b,cap):NativeRowTrait(0,"visible",b,cap);};
  build(sizeof(b)); unsigned len=std::strlen(b);
  for(unsigned cap=1;cap<=len;++cap) Check(!build(cap) && !*b,"truncation refused");
  Check(build(len+1),"exact capacity accepted");
 }
 TestResetSelected();
 {
  // A refused change is neither saved nor applied.
  obvr::Config laser; laser.hands.laserBeam=true; laser.hands.laserDot=false;
  Writer refusedWriter;
  NativeSettingEdit off; off.definition=FindSetting("Hands","LaserBeam"); off.value=0.0f;
  Check(CommitNativeEdit(off,laser,refusedWriter)==NativeEditResult::Refused,"beam off with no dot is refused");
  Check(refusedWriter.saves==0 && laser.hands.laserBeam,"and neither saved nor applied");
  laser.hands.laserDot=true;
  Check(CommitNativeEdit(off,laser,refusedWriter)==NativeEditResult::Saved && !laser.hands.laserBeam,
        "with the dot on it goes through");
 }
 std::printf("Native settings: %u definitions, %u pages, %d failures\n",visited,menu.Pages(),failures);
 return failures?1:0;
}
