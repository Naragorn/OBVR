#include <cstdio>
#include <cstring>
#include "ui/NativeSettings.h"
#include "core/AtomicFlag.h"
using namespace obvr::ui;
int failures=0;
void Check(bool b,const char* s) { if(!b) { ++failures; std::printf("FAIL %s\n",s); } }
struct Writer:NativeSettingWriter {
 bool success=true; int saves=0,recenters=0; char last[32]{};
 bool Save(const SettingDefinition&,const char* s) override { ++saves; std::strcpy(last,s); return success; }
 void Recenter() override { ++recenters; }
};
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
 std::printf("Native settings: %u definitions, %u pages, %d failures\n",visited,menu.Pages(),failures);
 return failures?1:0;
}
