#include <cstdio>
#include <limits>
#include <cmath>
#include "vr/ScrollSession.h"

using namespace obvr::vr::menu;
namespace {
int checks = 0, failures = 0;
void Check(bool ok, const char* text) {
	++checks; if (!ok) { ++failures; std::printf("FAIL: %s\n", text); }
}
ScrollSessionFrame Frame(ScrollOrientation orientation = ScrollOrientation::Horizontal) {
	ScrollSessionFrame f;
	f.orientation = orientation; f.bridgeAvailable = f.contextValid = true;
	f.gripKnown[0] = f.gripKnown[1] = true;
	f.gesture.head.tracked = f.gesture.left.tracked = f.gesture.right.tracked = true;
	f.gesture.eligible = f.gesture.controlsNeutral = true; f.gesture.dt = .05f;
	f.gesture.left.position = {-.06f,-.2f,-.35f};
	f.gesture.right.position = {.06f,-.2f,-.35f};
	if (orientation == ScrollOrientation::Vertical) {
		f.gesture.left.position = {0,-.26f,-.35f}; f.gesture.right.position = {0,-.14f,-.35f};
	}
	return f;
}
void Distance(ScrollSessionFrame& f, float distance) {
	if (f.orientation == ScrollOrientation::Vertical) {
		f.gesture.left.position.y = -.2f-distance/2;
		f.gesture.right.position.y = -.2f+distance/2;
	} else {
		f.gesture.left.position.x = -distance/2;
		f.gesture.right.position.x = distance/2;
	}
}
ScrollSessionResult Step(ScrollSessionState& s, ScrollSessionFrame& f) {
	f.gesture.controlsNeutral = !f.grip[0] && !f.grip[1];
	return StepScroll(s,f);
}
void Ready(ScrollSessionState& s, ScrollSessionFrame& f) {
	int cues = 0;
	for (int i=0;i<10;++i) cues += Step(s,f).readyCue ? 1 : 0;
	Check(s.phase == ScrollPhase::PoseDetected && s.readiness.ready && cues == 1,
	      "neutral deliberate dwell gives one ready cue");
}
void Grab(ScrollSessionState& s, ScrollSessionFrame& f) {
	Ready(s,f); f.grip[0] = true;
	const auto r=Step(s,f);
	Check(r.phase == ScrollPhase::Grabbed && r.ownsInput && !r.interactive &&
	      r.command == ScrollCommand::None, "one deliberate grip owns input without opening engine menu");
}
void Full(ScrollSessionState& s, ScrollSessionFrame& f) {
	Grab(s,f); f.grip[1] = true; Distance(f,.6f);
	const auto r=Step(s,f);
	Check(r.phase == ScrollPhase::Open && r.amount == 1 && r.fullCue && !r.interactive &&
	      r.command == ScrollCommand::None, "physical full opening alone never enables menu input");
	Check(!Step(s,f).fullCue, "resting full pose gives no repeated cue");
}
void Interact(ScrollSessionState& s, ScrollSessionFrame& f, int support=0) {
	Full(s,f); f.grip[1-support] = false;
	auto r=Step(s,f);
	Check(r.phase == ScrollPhase::HeldOpen && r.captureSupport && r.support == support &&
	      r.command == ScrollCommand::Open && !r.interactive, "ordered release requests menu and captures support");
	Check(Step(s,f).command == ScrollCommand::None, "pending engine open is requested once");
	f.menu=EngineMenu::Owned; f.menuGeneration=7; r=Step(s,f);
	Check(r.phase == ScrollPhase::Interacting && r.interactive, "engine acknowledgement permits interaction");
}
void TestCycle(ScrollOrientation orientation, int support) {
	ScrollSessionState s; auto f=Frame(orientation); Interact(s,f,support);
	Distance(f,.2f);
	Check(Step(s,f).amount == 1 && s.phase == ScrollPhase::Interacting,
	      "free hand reaching the menu preserves physically opened held scroll");
	f.grip[1-support]=true; f.freeEndReached=true;
	Check(Step(s,f).interactive, "regrab at partial physical separation cannot snap scroll");
	f.grip[1-support]=false; Step(s,f); Distance(f,.6f); f.freeEndReached=false;
	f.grip[1-support]=true;
	Check(Step(s,f).interactive, "full separation away from free end is not a regrab");
	f.grip[1-support]=false; Step(s,f); f.freeEndReached=true; f.grip[1-support]=true;
	auto r=Step(s,f);
	Check(r.phase == ScrollPhase::Open && !r.interactive && r.releaseSelection,
	      "valid regrab immediately releases drag and disables interaction");
	Distance(f,.34f); r=Step(s,f);
	Check(r.phase == ScrollPhase::RollingUp && r.amount > .49f && r.amount < .51f && !r.interactive,
	      "physical closing directly controls opening amount");
	Distance(f,.5f); r=Step(s,f);
	Check(r.phase == ScrollPhase::Unrolling && !r.interactive, "reversing direction reverses unroll without activation");
	Distance(f,.6f); Step(s,f); f.grip[1-support]=false; r=Step(s,f);
	Check(r.interactive && r.command == ScrollCommand::None, "releasing after reopening reuses same owned menu");
	f.grip[1-support]=true; Step(s,f); Distance(f,.08f); r=Step(s,f);
	Check(r.phase == ScrollPhase::Closing && r.command == ScrollCommand::Close && r.closedCue && !r.visible,
	      "physical closing asks engine to close once");
	r=Step(s,f); Check(r.command == ScrollCommand::None && r.ownsInput, "wait for close ack without repeat toggles");
	f.menu=EngineMenu::Closed; r=Step(s,f);
	Check(r.phase == ScrollPhase::AwaitRelease && r.ownsInput, "close ack still blocks held grips");
	f.grip[0]=f.grip[1]=false; r=Step(s,f);
	Check(r.phase == ScrollPhase::Inactive && !r.ownsInput && !r.interactive, "neutral returns gameplay ownership");
}
void TestOpening() {
	ScrollSessionState s; auto f=Frame();
	f.grip[0]=true;
	for(int i=0;i<20;++i) StepScroll(s,f);
	Check(s.phase==ScrollPhase::Inactive, "grip fields override a contradictory neutral flag");
	f.grip[0]=true; for(int i=0;i<20;++i) Step(s,f);
	Check(s.phase == ScrollPhase::Inactive, "preheld grip cannot arm gesture");
	f.grip[0]=false; Grab(s,f);
	for(int p=1;p<100;++p) {
		Distance(f,.08f+.52f*p/100); const auto r=Step(s,f);
		Check(!r.interactive && r.command == ScrollCommand::None && r.amount < 1,
		      "every partial percentage refuses menu input and native opening");
		Check(Step(s,f).amount == r.amount, "stationary hands never auto-complete");
	}
	Distance(f,.6f); f.grip[0]=false; f.grip[1]=true;
	auto r=Step(s,f);
	Check(!r.interactive && r.command == ScrollCommand::None && r.phase != ScrollPhase::HeldOpen,
	      "release on first full-distance sample cannot activate");
	f.grip[0]=true; Step(s,f); f.grip[0]=false; r=Step(s,f);
	Check(r.command == ScrollCommand::Open && r.support == 1, "later deliberate release activates normally");
	s={}; f=Frame(); Grab(s,f); Distance(f,.3f); Step(s,f); Distance(f,.08f); r=Step(s,f);
	Check(r.phase == ScrollPhase::Closing && r.command == ScrollCommand::None,
	      "partial open then close never opens native menu");
	s={}; f=Frame(); Full(s,f); f.grip[0]=false; Distance(f,.59f); r=Step(s,f);
	Check(r.phase != ScrollPhase::HeldOpen && r.command == ScrollCommand::None,
	      "shrinking on release below full refuses interaction");
	s={}; f=Frame(); Full(s,f); f.grip[1]=false; Step(s,f);
	f.freeEndReached=true; f.grip[1]=true; r=Step(s,f);
	Check(r.phase==ScrollPhase::HeldOpen && r.command==ScrollCommand::None,
	      "regrab before acknowledgement cannot issue duplicate open requests");
	f.grip[1]=false; r=Step(s,f);
	Check(r.command==ScrollCommand::None, "another release during pending open cannot toggle menu");
	s={}; f=Frame(); Grab(s,f); f.grip[1]=true;
	Distance(f,std::nextafter(.6f,0.0f)); r=Step(s,f);
	Check(r.phase!=ScrollPhase::Open && !r.fullCue, "session rejects one float below full distance");
	Distance(f,.08001f); r=Step(s,f);
	Check(r.phase==ScrollPhase::RollingUp && !r.closedCue, "close epsilon cannot eat measurable physical separation");
	s={}; f=Frame(); Ready(s,f); Distance(f,.3f); f.grip[0]=true; r=Step(s,f);
	Check(r.phase==ScrollPhase::Inactive && !r.ownsInput, "grip after leaving ready pose cannot capture controls");
}
void TestFailures() {
	for(int phase=0;phase<4;++phase) for(int fault=0;fault<12;++fault) {
		ScrollSessionState s; auto f=Frame();
		if(phase==0) Grab(s,f);
		if(phase==1) Full(s,f);
		if(phase==2) {Full(s,f); f.grip[1]=false; Step(s,f);}
		if(phase==3) Interact(s,f);
		switch(fault) {
		case 0: f.gesture.left.tracked=false; break;
		case 1: f.gesture.right.tracked=false; break;
		case 2: f.gesture.head.tracked=false; break;
		case 3: f.gripKnown[0]=false; f.grip[0]=false; break;
		case 4: f.gripKnown[1]=false; f.grip[1]=false; break;
		case 5: f.contextValid=false; break;
		case 6: f.bridgeAvailable=false; break;
		case 7: f.cancel=true; break;
		case 8: f.orientation=ScrollOrientation::Off; break;
		case 9: f.orientation=ScrollOrientation::Vertical; break;
		case 10: f.gesture.dt=.2f; break;
		case 11: f.menu=EngineMenu::Foreign; break;
		}
		const auto r=Step(s,f);
		Check(r.phase==ScrollPhase::Closing && !r.interactive && r.releaseSelection,
		      "each active phase cancels safely on tracking action context timing or ownership loss");
		Check(r.command == (phase==2 ? ScrollCommand::CancelPendingOpen :
		      phase==3 && fault!=11 ? ScrollCommand::Close : ScrollCommand::None),
		      "cleanup only requests close for the owned menu or cancels its pending open");
	}
	ScrollSessionState s; auto f=Frame(); Interact(s,f);
	f.menuGeneration=8; auto r=Step(s,f);
	Check(r.phase==ScrollPhase::Closing && r.command==ScrollCommand::None,
	      "replaced menu generation refuses input and is never closed");
	s={}; f=Frame(); Interact(s,f); f.grip[0]=false; f.grip[1]=true; r=Step(s,f);
	Check(r.phase==ScrollPhase::Closing, "support release never silently switches hands");
	s={}; f=Frame(); Full(s,f); f.grip[0]=f.grip[1]=false; r=Step(s,f);
	Check(r.phase==ScrollPhase::Closing && r.command==ScrollCommand::None, "both grips released cannot activate");
	s={}; f=Frame(); Full(s,f); f.grip[1]=false; Step(s,f);
	for(int i=0;i<25 && s.phase!=ScrollPhase::Closing;++i) r=Step(s,f);
	Check(r.phase==ScrollPhase::Closing && r.command==ScrollCommand::CancelPendingOpen,
	      "missing open ack cancels pending request after bounded timeout");
	s={}; f=Frame(); Interact(s,f); f.cancel=true; Step(s,f); f.cancel=false;
	for(int i=0;i<25 && s.phase!=ScrollPhase::AwaitRelease;++i) Step(s,f);
	Check(s.phase==ScrollPhase::AwaitRelease, "failed close falls back without permanent controller ownership");
	f.grip[0]=f.grip[1]=false; f.gripKnown[1]=false; Step(s,f);
	Check(s.phase==ScrollPhase::AwaitRelease, "missing action cannot be mistaken for neutral after closing");
	f.gripKnown[1]=true; r=Step(s,f);
	Check(!r.ownsInput, "neutral restores legacy input even when native close failed");
	s={}; f=Frame(); Interact(s,f); f.menu=EngineMenu::Closed; r=Step(s,f);
	Check(r.phase==ScrollPhase::Closing && r.command==ScrollCommand::None,
	      "external legacy close releases scroll without reopening or another close");
	Step(s,f); f.grip[0]=false; f.gesture.left.tracked=false; Step(s,f);
	Check(s.phase==ScrollPhase::AwaitRelease, "untracked neutral cannot restore controller gameplay");
	f.gesture.left.tracked=true; Check(!Step(s,f).ownsInput, "tracked neutral recovers controller gameplay");
	s={}; f=Frame(); Interact(s,f); f.grip[1]=true; f.freeEndReached=true; Step(s,f);
	Distance(f,.3f); Step(s,f); f.menuGeneration=9; r=Step(s,f);
	Check(r.phase==ScrollPhase::Closing && r.command==ScrollCommand::None,
	      "menu replacement while rolling refuses stale close");
	s={}; f=Frame(); Grab(s,f); f.menu=EngineMenu::Owned; f.menuGeneration=3; r=Step(s,f);
	Check(r.phase==ScrollPhase::Closing && r.command==ScrollCommand::None,
	      "unrequested native menu during unrolling is not owned by scroll");
	s={}; f=Frame(); Interact(s,f); f.cancel=true; Step(s,f); f.bridgeAvailable=false;
	Check(Step(s,f).phase==ScrollPhase::AwaitRelease, "bridge failure cannot block close acknowledgement forever");
	for(int fault=0;fault<8;++fault) {
		s={}; f=Frame(); Ready(s,f);
		switch(fault) {
		case 0: f.menu=EngineMenu::Foreign; break;
		case 1: f.menu=EngineMenu::Owned; break;
		case 2: f.bridgeAvailable=false; break;
		case 3: f.contextValid=false; break;
		case 4: f.cancel=true; break;
		case 5: f.orientation=ScrollOrientation::Off; break;
		case 6: f.gesture.dt=std::numeric_limits<float>::quiet_NaN(); break;
		case 7: f.gripKnown[0]=false; break;
		}
		r=Step(s,f);
		Check(r.phase==ScrollPhase::Inactive && !r.ownsInput, "invalid ready pose cannot steal gameplay input");
	}
}
}
int main() {
	TestCycle(ScrollOrientation::Horizontal,0); TestCycle(ScrollOrientation::Horizontal,1);
	TestCycle(ScrollOrientation::Vertical,0); TestCycle(ScrollOrientation::Vertical,1);
	TestOpening(); TestFailures();
	std::printf("Scroll session: %d/%d checks passed\n",checks-failures,checks);
	return failures ? 1 : 0;
}
