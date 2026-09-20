#include <cstdio>
#include <initializer_list>
#include "ui/NativeOnboarding.h"
#include "game/MenuQueSignature.h"
using namespace obvr::ui;
int failures=0;
void Check(bool yes,const char* label) { if(!yes) { ++failures; std::printf("FAIL %s\n",label); } }
struct Port: NativeMenuPort {
 bool openOK=true, saveOK=true, selectedMotion=false;
 int opens=0,saves=0;
 NativePage last=NativePage::Choices;
 bool Open(NativePage p) override { ++opens; last=p; return openOK; }
 bool SaveMode(bool fullVR) override { ++saves; selectedMotion=fullVR; return saveOK; }
};
int main() {
 for(int selected: {-1,kNativeClassic,kNativeMotion}) for(int flags=0;flags<8;++flags) {
  const int expected=(flags&4)?-1:(flags&3)?kNativeClassic:selected;
  Check(NavigateOnboarding(selected,flags&1,flags&2,flags&4)==expected,"native keyboard focus, mouse takeover and simultaneous directions");
 }

 { Port p; NativeOnboarding n;
   n.Tick(false,false,-1,p); Check(p.opens==0,"wait for main menu");
   n.Tick(true,true,-1,p); Check(p.opens==0 && n.State()==NativeState::Waiting,"do not replace foreign menu");
   n.Tick(true,false,-1,p); Check(n.State()==NativeState::Open && p.opens==1,"open once at main menu");
   n.Tick(false,false,-1,p); Check(p.opens==1,"idle open frame");
   n.Tick(true,false,kNativeClassic,p); Check(n.State()==NativeState::Done && p.saves==1,"classic saves");
   n.Tick(true,false,kNativeClassic,p); Check(p.saves==1,"completed is inert"); }
 { Port p; p.openOK=false; NativeOnboarding n; n.Tick(true,false,-1,p);
   Check(n.State()==NativeState::Failed,"initial open fails");
   n.Tick(true,false,-1,p); Check(p.opens==1,"failed is inert"); }
 { Port p; NativeOnboarding n; n.Tick(true,false,-1,p); p.saveOK=false;
   n.Tick(true,false,kNativeClassic,p); Check(p.last==NativePage::SaveFailed && n.State()==NativeState::Open,"save failure offers retry");
   p.saveOK=true; n.Tick(true,false,kNativeClassic,p); Check(n.State()==NativeState::Done && p.saves==2,"retry succeeds"); }
 { Port p; NativeOnboarding n; n.Tick(true,false,-1,p); p.saveOK=false; p.openOK=false;
   n.Tick(true,false,kNativeClassic,p); Check(n.State()==NativeState::Failed,"save failure page also fails"); }
 { Port p;NativeOnboarding n;n.Tick(true,false,-1,p);p.saveOK=false;
   n.Tick(true,false,kNativeClassic,p);Check(n.State()==NativeState::Open && p.last==NativePage::SaveFailed,"classic mode can retry a failed save");
   p.saveOK=true;n.Tick(true,false,kNativeClassic,p);
   Check(n.State()==NativeState::Done && !p.selectedMotion,"classic choice saves the seated mode");
   n.Tick(true,false,kNativeClassic,p);Check(p.saves==2,"completed mode choice cannot repeat"); }
 { Port p;NativeOnboarding n;n.Tick(true,false,-1,p);
   n.Tick(false,false,kNativeMotion,p);
   Check(n.State()==NativeState::Open && p.saves==0,"full VR button is refused while under construction");
   n.Tick(false,false,kNativeClassic,p);
   Check(n.State()==NativeState::Done && p.saves==1 && !p.selectedMotion,
         "classic mode remains selectable after a refused full VR report"); }
 { Port p; NativeOnboarding n; n.Tick(true,false,-1,p); n.Tick(true,false,9103,p);
   Check(n.State()==NativeState::Failed && p.saves==0,"removed later ID refused"); }
 { Port p; NativeOnboarding n; n.Tick(true,false,-1,p); n.Tick(false,true,kNativeClassic,p);
   Check(n.State()==NativeState::Failed && p.saves==0,"foreign menu cannot choose a mode"); }
 { Port p; NativeOnboarding n; n.Tick(true,false,-1,p); n.Tick(false,false,42,p);
   Check(n.State()==NativeState::Failed && p.saves==0,"unknown id refused"); }
 { Port p; NativeOnboarding n; n.Tick(true,false,-1,p); n.Tick(true,false,-1,p);
   Check(n.State()==NativeState::Failed,"external close yields control"); }
 // All bytes, including all relocated operands, are checked individually.
 unsigned char bytes[]={0x55,0x8B,0xEC,0x8B,0x45,8,0x8B,0x0D,0xD8,0x0E,5,0x10,
 0x89,8,0x80,0x3D,0xD4,0x0E,5,0x10,0,0x74,0x0B,0xC6,5,0xD4,0x0E,5,0x10,0,0xB0,1,0x5D,0xC3,0x32,0xC0,0x5D,0xC3};
 Check(obvr::game::MenuQuePollMatches(bytes,0x10000000),"verified poll bytes");
 Check(!obvr::game::MenuQuePollMatches(nullptr,0),"null bytes");
 Check(!obvr::game::MenuQuePollMatches(bytes,0x20000000),"wrong relocation refused");
 for(unsigned i=0;i<sizeof(bytes);++i) { bytes[i]^=1; Check(!obvr::game::MenuQuePollMatches(bytes,0x10000000),"every byte mismatch refused"); bytes[i]^=1; }
 for(unsigned i: {8u,16u,25u}) bytes[i+3]=0x20;
 Check(obvr::game::MenuQuePollMatches(bytes,0x20000000),"relocated module accepted");
 std::printf("Native onboarding: %d failures\n",failures);
 return failures?1:0;
}
