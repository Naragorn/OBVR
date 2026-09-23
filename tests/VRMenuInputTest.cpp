#include <cstdio>
#include <limits>
#include "game/VRMenuInput.h"

using namespace obvr::game::vrmenu;
namespace {
int checks=0, failures=0;
void Check(bool ok,const char* label) {
	++checks; if(!ok) {++failures; std::printf("FAIL: %s\n",label);}
}
Snapshot Menu() {return {true,true,true,true,{1002,0x1234,5},1280,720};}
Input Point(bool held=false) {return {Menu().identity,true,100,200,held,0,false};}
void Press(InputQueue& q) {
	Check(q.Publish(Point()),"queue initial neutral");
	Check(q.Consume(Menu()).accepted,"deliver neutral to arm");
	Check(q.Publish(Point(true)),"queue press");
	const auto d=q.Consume(Menu());
	Check(d.selectDown && d.selectHeld && !d.selectUp,"deliver one down edge");
}
void TestOrder() {
	InputQueue q;
	q.Publish(Point(true));
	Check(!q.Consume(Menu()).selectDown,"held input at activation cannot select");
	Press(q);
	auto drag=Point(true); drag.x=400; drag.y=500; q.Publish(drag);
	auto d=q.Consume(Menu());
	Check(d.cursor && d.x==400 && d.y==500 && d.selectHeld && !d.selectDown,
	      "drag moves cursor while preserving held selection");
	q.Publish(Point(false)); q.Publish(Point(true)); q.Publish(Point(false));
	d=q.Consume(Menu()); Check(d.selectUp && !d.selectHeld && q.Pending()==2,"release precedes next press");
	d=q.Consume(Menu()); Check(d.selectDown && d.selectHeld && q.Pending()==1,"next poll delivers press");
	d=q.Consume(Menu()); Check(d.selectUp && !d.selectHeld && q.Pending()==0,"following poll delivers release");
	Check(!q.Consume(Menu()).accepted,"empty queue has no fabricated input");
	for(unsigned i=0;i<InputQueue::Capacity*3;++i) {
		auto p=Point(); p.x=static_cast<float>(i);
		Check(q.Publish(p),"ring wraps with capacity intact");
		Check(q.Consume(Menu()).x==i,"ring preserves sample ordering after wrap");
	}
	for(int wheel=-4;wheel<=4;++wheel) {
		auto p=Point(); p.wheel=wheel; q.Publish(p);
		Check(q.Consume(Menu()).wheel==wheel,"signed wheel notches delivered");
	}
	Press(q); auto cancel=Point(true); cancel.cancel=true;
	q.Publish(cancel); q.Publish(Point(true)); d=q.Consume(Menu());
	Check(d.cancel && d.selectUp && !d.selectHeld && !q.Pending(),"cancel releases and discards following input");
	q.Publish(Point(true)); Check(!q.Consume(Menu()).selectDown,"cancel needs fresh neutral before another click");
}
void TestLoss() {
	for(int fault=0;fault<12;++fault) {
		InputQueue q; Press(q); q.Publish(Point(true));
		auto s=Menu();
		switch(fault) {
		case 0:s.available=false;break;
		case 1:s.focused=false;break;
		case 2:s.open=false;break;
		case 3:s.interactive=false;break;
		case 4:++s.identity.type;break;
		case 5:++s.identity.root;break;
		case 6:++s.identity.generation;break;
		case 7:s.identity={};break;
		case 8:s.width=0;break;
		case 9:s.height=16385;break;
		case 10:s.width=std::numeric_limits<float>::quiet_NaN();break;
		case 11:s.height=std::numeric_limits<float>::infinity();break;
		}
		auto d=q.Consume(s);
		Check(d.selectUp && !d.accepted && !q.Held() && !q.Pending(),"context/identity loss releases and flushes");
		q.Publish(Point(true)); Check(!q.Consume(Menu()).selectDown,"restored context cannot reuse held input");
	}
	for(int fault=0;fault<13;++fault) {
		InputQueue q; Press(q); auto p=Point(true);
		switch(fault) {
		case 0:++p.target.type;break;
		case 1:++p.target.root;break;
		case 2:++p.target.generation;break;
		case 3:p.target={};break;
		case 4:p.x=-1;break;
		case 5:p.y=-1;break;
		case 6:p.x=1280;break;
		case 7:p.y=720;break;
		case 8:p.x=std::numeric_limits<float>::quiet_NaN();break;
		case 9:p.y=std::numeric_limits<float>::infinity();break;
		case 10:p.wheel=5;break;
		case 11:p.wheel=-5;break;
		case 12:p.cursor=false;break;
		}
		q.Publish(p); q.Publish(Point(true)); const auto d=q.Consume(Menu());
		Check(d.selectUp && !d.accepted && !q.Pending(),"invalid packet cannot select and flushes stale following input");
	}
	InputQueue q; Press(q);
	for(unsigned i=0;i<InputQueue::Capacity;++i) Check(q.Publish(Point(true)),"fill bounded queue");
	Check(!q.Publish(Point(false)) && !q.Pending(),"queue overflow rejects latest and flushes all");
	q.Publish(Point(true));
	auto d=q.Consume(Menu());
	Check(d.selectUp && !d.accepted && !q.Pending(),"overflow retains mandatory release outside queue capacity");
	Check(!q.Consume(Menu()).selectUp,"queue release edge emitted only once");
	Press(q); d=q.Release();
	Check(d.selectUp && !q.Held(),"watchdog or native dispatch failure explicitly releases");
	Press(q); q.Cancel(); d=q.Consume(Menu());
	Check(d.selectUp && !q.Pending(),"producer tracking loss cancels with empty queue too");
	auto release=Point(); release.cursor=false; q.Publish(release);
	Check(q.Consume(Menu()).accepted,"cursor miss still permits neutral release");
	auto wheel=release; wheel.wheel=1; q.Publish(wheel);
	Check(!q.Consume(Menu()).accepted,"wheel without a surface hit refuses");
	auto edge=Point();edge.x=1279;edge.y=719;q.Publish(edge);
	Check(q.Consume(Menu()).accepted,"last valid pixel is accepted");
	edge.x=edge.y=0;q.Publish(edge);Check(q.Consume(Menu()).accepted,"first valid pixel is accepted");
}
void TestRequests() {
	auto s=Menu(); Request open{8,Operation::OpenPersonalMenu,{}};
	Check(!CanDispatch(open,s),"opening refuses existing menu");
	s=Snapshot{true,true,false,false,{},0,0};
	Check(CanDispatch(open,s),"closed world permits one explicit opening request");
	Acknowledgement a{8,Operation::OpenPersonalMenu,false,Menu().identity};
	Check(!Acknowledged(open,a),"command dispatch is not an engine acknowledgement");
	a.observed=true;Check(Acknowledged(open,a),"observed matching open acknowledges");
	Request close{9,Operation::CloseOwnedMenu,Menu().identity};
	Check(!CanDispatch(close,s),"cannot close an absent menu");
	s=Menu();Check(CanDispatch(close,s),"owned identity permits close");
	++s.identity.generation;Check(!CanDispatch(close,s),"stale close cannot affect replacement menu");
	a={9,Operation::CloseOwnedMenu,true,close.expected};Check(Acknowledged(close,a),"matching observed close acknowledges");
	s=Snapshot{true,true,false,false,{},0,0};
	Check(CanDispatch(open,s),"closed world with no identity or cursor extent can open");
	InputQueue closedInput; closedInput.Publish(Point());
	Check(!closedInput.Consume(s).accepted,"closed world without extent refuses cursor input");
	s=Snapshot{true,true,false,false,close.expected,0,0};
	Check(!CanDispatch(open,s),"stale closed identity refuses opening request");
	s=Snapshot{true,true,true,false,close.expected,0,0};
	Check(CanDispatch(close,s),"owned lifecycle can close without interaction or cursor extent");
	++s.identity.root;Check(!CanDispatch(close,s),"foreign identity cannot close owned menu");
	Identity partial{close.expected.type,0,close.expected.generation};
	s=Snapshot{true,true,true,false,partial,0,0};
	Request partialClose{9,Operation::CloseOwnedMenu,partial};
	Check(!CanDispatch(partialClose,s),"partial identity cannot close owned menu");
	s=Snapshot{false,true,true,false,close.expected,0,0};
	Check(!CanDispatch(close,s),"missing bridge refuses close request");
	s=Snapshot{true,false,true,false,close.expected,0,0};
	Check(!CanDispatch(close,s),"unfocused bridge refuses close request");
	for(int fault=0;fault<6;++fault) {
		a={9,Operation::CloseOwnedMenu,true,close.expected};
		switch(fault) {
		case 0:a.token=10;break;
		case 1:a.operation=Operation::OpenPersonalMenu;break;
		case 2:a.observed=false;break;
		case 3:++a.identity.type;break;
		case 4:++a.identity.root;break;
		case 5:++a.identity.generation;break;
		}
		Check(!Acknowledged(close,a),"unrelated acknowledgement cannot satisfy request");
	}
	s=Menu();s.open=false;s.available=false;Check(!CanDispatch(open,s),"missing bridge refuses request");
	s.available=true;s.focused=false;Check(!CanDispatch(open,s),"unfocused bridge refuses request");
	s.focused=true;open.token=0;Check(!CanDispatch(open,s),"zero token refuses request");
	a={0,Operation::OpenPersonalMenu,true,Menu().identity};Check(!Acknowledged(open,a),"zero token cannot acknowledge");
	open.token=8;a.token=8;a.identity={};Check(!Acknowledged(open,a),"open ack needs observed identity");
	open.operation=Operation::None;a.operation=Operation::None;
	Check(!CanDispatch(open,s) && !Acknowledged(open,a),"no-operation cannot dispatch or acknowledge");
}

void TestGuardPaths() {
	InputQueue q;
	auto forced = q.Release();
	Check(forced.selectUp && !forced.accepted,"explicit release is safe when already neutral");
	for(unsigned i=0;i<InputQueue::Capacity;++i) Check(q.Publish(Point()),"fill queue without a held selection");
	Check(!q.Publish(Point()) && !q.Pending() && !q.Held(),"overflow flushes an unheld queue without fabricating a release");
	Check(!q.Consume(Menu()).accepted,"unheld overflow leaves no packet behind");

	Press(q); q.Cancel();
	Check(!q.Publish(Point()) && !q.Pending(),"pending release rejects new producer packets");
	auto d=q.Consume(Menu());
	Check(d.selectUp && !d.accepted,"pending release is emitted before producer recovery");

	Press(q); auto invalidCancel=Point(true); invalidCancel.cancel=true; invalidCancel.target={};
	q.Publish(invalidCancel); d=q.Consume(Menu());
	Check(d.selectUp && !d.cancel && !d.accepted,"invalid cancel follows identity loss path");

	q.Publish(Point()); q.Consume(Menu());
	auto cancel=Point(); cancel.cancel=true; q.Publish(cancel); d=q.Consume(Menu());
	Check(d.cancel && !d.selectUp && !d.accepted,"neutral cancel still flushes without a release edge");

	Request open{1,Operation::OpenPersonalMenu,Menu().identity};
	Check(!CanDispatch(open,Snapshot{true,true,false,true,Menu().identity,1280,720}),
	      "open request with an expected identity is refused");
	Acknowledgement ack{1,Operation::OpenPersonalMenu,true,Menu().identity};
	Check(!Acknowledged(open,ack),"open acknowledgement requires an empty expected identity");
}
}
int main() {
	TestOrder();TestLoss();TestRequests();TestGuardPaths();
	std::printf("VR menu input: %d/%d checks passed\n",checks-failures,checks);
	return failures ? 1 : 0;
}
