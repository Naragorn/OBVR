#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <initializer_list>
#include "ui/NativeHandAdjust.h"
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
 void AdjustHands() override { ++adjusts; }
 int adjusts=0;
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
 Check(editable+2==visited,"reset test includes the two action rows' refusal paths");
 Check(refusedAtDefault==editable,"every editable definition refuses reset at default");
}

const char* Key(const SettingDefinition* d) { return d ? d->iniKey : "(none)"; }
bool SameKeys(NativeSettings& menu,const Config& c,std::initializer_list<const char*> keys) {
 (void)c;
 unsigned index=0; bool same=true;
 for(unsigned page=0;page<menu.Pages();++page) {
  for(unsigned slot=0;slot<kNativeSettingsRows;++slot) {
   const auto* d=menu.Row(slot);
   if(!d) continue;
   if(index>=keys.size() || std::strcmp(d->iniKey,keys.begin()[index])!=0) {
    std::printf("        row %u is %s\n",index,Key(d)); same=false;
   }
   ++index;
  }
  menu.Click(kNativeNext,c);
 }
 return same && index==keys.size();
}

void TestComfortView() {
 std::printf("Comfort page view\n");
 // Every combination of the three rows the others follow.
 for(unsigned mask=0;mask<8;++mask) {
  Config c;
  c.look.snapTurning=mask&1; c.look.snapTurnInstant=mask&2; c.look.snapTurnVignette=mask&4;
  const bool snap=mask&1, instant=mask&2, vignette=mask&4;
  unsigned shown=0;
  for(UInt32 i=0;i<SettingDefinitionCount();++i) {
   const auto& d=SettingDefinitions()[i];
   const bool all=SettingShownIn(SettingsView::All,d,c);
   const bool comfort=SettingShownIn(SettingsView::Comfort,d,c);
   Check(all,"the whole menu shows every row");
   bool expected=false;
   if(!std::strcmp(d.iniKey,"LeftHanded") || !std::strcmp(d.iniKey,"SnapTurning")) expected=true;
   else if(!std::strcmp(d.iniKey,"SnapTurnAngle") || !std::strcmp(d.iniKey,"SnapTurnInstant") ||
           !std::strcmp(d.iniKey,"SnapTurnVignette")) expected=snap;
   else if(!std::strcmp(d.iniKey,"SnapTurnSpeed")) expected=snap && !instant;
   else if(!std::strcmp(d.iniKey,"SnapTurnVignetteRadius") ||
           !std::strcmp(d.iniKey,"SnapTurnVignetteStrength")) expected=snap && vignette;
   Check(comfort==expected,"a comfort row shows exactly when the row it follows is on");
   shown+=comfort;
  }
  const unsigned want=2+(snap ? 3+(instant?0:1)+(vignette?2:0) : 0);
  Check(shown==want,"the comfort page's row count for each combination");
 }
 // A section that matches but a key that does not, and the other way round.
 SettingDefinition fake; fake.iniSection="Hands"; fake.iniKey="LeftHande";
 Config c;
 Check(!SettingShownIn(SettingsView::Comfort,fake,c),"a key prefix is not the key");
 fake.iniSection="Look"; fake.iniKey="LeftHanded";
 Check(!SettingShownIn(SettingsView::Comfort,fake,c),"the right key in the wrong section is not it");

 // Snap off: two rows, one page, left-handed and snap turning in table order.
 Config off; off.look.snapTurning=false;
 NativeSettings menu; menu.SetView(SettingsView::Comfort,off);
 Check(menu.View()==SettingsView::Comfort && menu.Pages()==1 && menu.First()==0,"snap off is one page");
 Check(SameKeys(menu,off,{"SnapTurning","LeftHanded"}),"snap off offers snap turning and left-handed");
 Check(std::strcmp(SettingDefinitions()[menu.Selected()].iniKey,"SnapTurning")==0,"the first row starts selected");
 Check(!NativePagingShown(menu.Pages()) && NativePagingShown(2) && !NativePagingShown(0),
       "one page hides Previous and Next; two show them");
 Check(kXmlBool[0]==1 && kXmlBool[1]==2,"XML booleans are &false; 1 and &true; 2, never 0 and 1");

 // Turning snap on through the page itself: the follow-up rows appear.
 Writer writer;
 const int snapPlus=kNativeRowBase+2;
 const auto edit=menu.Click(snapPlus,off);
 Check(edit.definition && !std::strcmp(edit.definition->iniKey,"SnapTurning") && edit.value==1.0f,"plus on snap turning proposes on");
 Check(CommitNativeEdit(edit,off,writer)==NativeEditResult::Saved && off.look.snapTurning,"saved and applied");
 menu.Sync(off);
 // The defaults: instant on, vignette on - no speed row.
 Check(SameKeys(menu,off,{"SnapTurning","SnapTurnAngle","SnapTurnInstant","SnapTurnVignette",
                          "SnapTurnVignetteRadius","SnapTurnVignetteStrength","LeftHanded"}),
       "snap on offers the follow-ups in table order");
 Check(!std::strcmp(SettingDefinitions()[menu.Selected()].iniKey,"SnapTurning"),"the edited row stays selected");

 // Everything on and eased: eight rows, two pages; the last row on page two.
 Config full; full.look.snapTurning=true; full.look.snapTurnInstant=false; full.look.snapTurnVignette=true;
 NativeSettings paged; paged.SetView(SettingsView::Comfort,full);
 Check(paged.Pages()==2,"eight rows need two pages");
 paged.Click(kNativeNext,full);
 Check(paged.First()==7 && paged.Row(0) && !std::strcmp(paged.Row(0)->iniKey,"LeftHanded") && !paged.Row(1),
       "the second page holds the last row");
 Check(!std::strcmp(SettingDefinitions()[paged.Selected()].iniKey,"LeftHanded"),"paging selects the page's first row");
 // On page two when snap goes off: the page vanishes, the menu comes back
 // to the last page there is, and the still-shown selection stays.
 full.look.snapTurning=false;
 paged.Sync(full);
 Check(paged.Pages()==1 && paged.First()==0,"a vanished page is left for the last one");
 Check(!std::strcmp(SettingDefinitions()[paged.Selected()].iniKey,"LeftHanded"),"its shown selection stays");
 // Selecting a follow-up row, then switching its parent off: the selection
 // moves to the page's first row.
 full.look.snapTurning=true;
 paged.Sync(full);
 paged.Click(kNativeRowBase+6*3,full); // the vignette darkness row
 Check(!std::strcmp(SettingDefinitions()[paged.Selected()].iniKey,"SnapTurnVignetteStrength"),"a follow-up row selected");
 full.look.snapTurnVignette=false;
 paged.Sync(full);
 Check(!std::strcmp(SettingDefinitions()[paged.Selected()].iniKey,"SnapTurning"),"a hidden selection moves to the page's first row");
 full.look.snapTurning=false;
 paged.Sync(full);
 // A selection that stays shown stays selected.
 paged.Click(kNativeRowBase+1*3,full);
 const UInt32 kept=paged.Selected();
 paged.Sync(full);
 Check(paged.Selected()==kept && !std::strcmp(SettingDefinitions()[kept].iniKey,"LeftHanded"),"a shown selection survives a sync");
 Check(paged.CanResetSelected(full)==CanResetSetting(SettingDefinitions()[kept],full),"reset follows the selected row");

 // Back to the whole menu: every row, from the first.
 paged.SetView(SettingsView::All,full);
 Check(paged.View()==SettingsView::All && paged.First()==0 && paged.Selected()==0 &&
       paged.Pages()==(SettingDefinitionCount()+kNativeSettingsRows-1)/kNativeSettingsRows,"the whole menu again");
}

void TestHandAdjust() {
 std::printf("Adjusting the hands: the window's choices and the session\n");
 using namespace obvr::ui;
 Check(ChooseHandAdjust(HandAdjustPage::Guide,kAdjustHandsPrimary)==HandAdjustChoice::Start &&
       ChooseHandAdjust(HandAdjustPage::Guide,kAdjustHandsSecondary)==HandAdjustChoice::Cancel,
       "the guide starts or cancels");
 Check(ChooseHandAdjust(HandAdjustPage::Guide,kAdjustHandsReset)==HandAdjustChoice::None &&
       ChooseHandAdjust(HandAdjustPage::Guide,-1)==HandAdjustChoice::None &&
       ChooseHandAdjust(HandAdjustPage::Guide,kAdjustHandsClose)==HandAdjustChoice::None,
       "the guide ignores the reset, no click and the hidden close");
 Check(ChooseHandAdjust(HandAdjustPage::Finish,kAdjustHandsPrimary)==HandAdjustChoice::Keep &&
       ChooseHandAdjust(HandAdjustPage::Finish,kAdjustHandsSecondary)==HandAdjustChoice::Again &&
       ChooseHandAdjust(HandAdjustPage::Finish,kAdjustHandsReset)==HandAdjustChoice::Reset,
       "the finish keeps, adjusts again or resets");
 Check(ChooseHandAdjust(HandAdjustPage::Finish,-1)==HandAdjustChoice::None,"the finish ignores no click");

 HandAdjustSession s;
 Check(!StepHandAdjustSession(s,true,true,false,1.0f) && !s.rightDone,"no session: nothing counts");
 StartHandAdjustSession(s);
 Check(s.active && !s.rightDone && !s.leftDone,"started clean");
 Check(!StepHandAdjustSession(s,true,false,false,5.0f) && s.rightDone,"one hand is not enough");
 Check(!StepHandAdjustSession(s,false,true,true,5.0f) && s.leftDone,"both done, but a grip is closed");
 Check(!StepHandAdjustSession(s,false,false,false,1.0f),"resting, not long enough yet");
 Check(!StepHandAdjustSession(s,false,false,true,1.0f) && s.quietSeconds==0.0f,"a grip closing starts the rest again");
 Check(!StepHandAdjustSession(s,false,false,false,1.0f),"one second of rest");
 Check(StepHandAdjustSession(s,false,false,false,0.6f) && s.finishAsked,"a second and a half: the finish");
 Check(!StepHandAdjustSession(s,false,false,false,5.0f),"asked once");
 StartHandAdjustSession(s);
 Check(!s.finishAsked && !s.rightDone,"adjusting again starts over");
 StepHandAdjustSession(s,true,true,false,0.0f);
 Check(!StepHandAdjustSession(s,false,false,false,0.0f) && s.quietSeconds==0.0f,"a frame without time adds no rest");

 obvr::Config config; NativeSettings menu; Writer writer;
 bool fired=false;
 for(unsigned page=0;page<menu.Pages() && !fired;++page) {
  for(unsigned slot=0;slot<7;++slot) {
   const auto* d=menu.Row(slot);
   if(d && d->action==SettingAction::AdjustHands) {
    const auto edit=menu.Click(kNativeRowBase+static_cast<int>(slot)*3+2,config);
    Check(CommitNativeEdit(edit,config,writer)==NativeEditResult::Action && writer.adjusts==1 &&
          writer.recenters==0 && writer.saves==0,"the Adjust hands row hands over to the guide, saves nothing");
    fired=true;
    break;
   }
  }
  if(!fired) menu.Click(kNativeNext,config);
 }
 Check(fired,"the Adjust hands row is in the menu");
}

void TestUpdateWindow() {
 std::printf("Update window\n");
 // Not shown yet: every combination of the four inputs.
 for(unsigned mask=0;mask<16;++mask) {
  const bool known=mask&1, main=mask&2, root=mask&4, busy=mask&8;
  UpdateWindowState s;
  const auto step=StepUpdateWindow(s,known,main,root,busy);
  const auto expected=!known ? UpdateWindowStep::None : !main ? UpdateWindowStep::None
                    : (root||busy) ? UpdateWindowStep::Wait : UpdateWindowStep::Open;
  Check(step==expected,"no answer or no main menu: nothing; a menu or the walkthrough: wait; else open");
  Check(s.shown==(expected==UpdateWindowStep::Open) && !s.done,"only an opening marks it shown");
 }
 // The whole life: open, up, OK, never again - not even back in the main menu.
 UpdateWindowState s;
 Check(StepUpdateWindow(s,true,true,false,false)==UpdateWindowStep::Open,"opens");
 Check(StepUpdateWindow(s,true,true,true,false)==UpdateWindowStep::Wait,"stays while it is up");
 Check(StepUpdateWindow(s,true,false,true,false)==UpdateWindowStep::Wait,"whatever is on top");
 Check(StepUpdateWindow(s,true,true,false,false)==UpdateWindowStep::None && s.done,"OK closes it for good");
 for(unsigned mask=0;mask<16;++mask)
  Check(StepUpdateWindow(s,mask&1,mask&2,mask&4,mask&8)==UpdateWindowStep::None,"a closed notice never comes back");
 // An answer that comes while the game is running waits for the main menu.
 UpdateWindowState late;
 Check(StepUpdateWindow(late,true,false,false,false)==UpdateWindowStep::None && !late.shown,
       "in the game: nothing, and nothing spent");
 Check(StepUpdateWindow(late,true,true,false,false)==UpdateWindowStep::Open,"back in the main menu: shown");
}

void TestComfortPageStep() {
 std::printf("Comfort page opening\n");
 for(unsigned mask=0;mask<8;++mask) {
  const bool pending=mask&1, root=mask&2, main=mask&4;
  const auto step=StepComfortPage(pending,root,main);
  const auto expected=!pending ? ComfortPageStep::None : root ? ComfortPageStep::Wait
                    : main ? ComfortPageStep::Open : ComfortPageStep::Skip;
  Check(step==expected,"nothing pending, wait for any generic menu, open over the main menu, else skip");
 }
}

int main() {
 TestComfortView();
 TestComfortPageStep();
 TestUpdateWindow();
 TestHandAdjust();
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
 Check(NativeTextCommand("user1","a; b",b,sizeof(b)) && std::strcmp(b,"SetMenuStringValue \"user1@a, b\" 1011")==0,"a semicolon becomes a comma: the compiler would end the line there");
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
