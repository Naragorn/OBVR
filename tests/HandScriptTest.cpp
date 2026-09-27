// Checks the hand script: reading it, walking it on the frames' clock, and
// the controller state a scripted hand becomes.

#include <cmath>
#include <cstdio>

#include "test/HandScript.h"
#include "test/HandScriptPose.h"

using namespace obvr;
using namespace obvr::test;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float tolerance = 1e-3f) { return std::fabs(a - b) <= tolerance; }

bool Parses(const char* text, HandScript& script) {
	ScriptParseError error;
	return ParseHandScript(text, script, error);
}

UInt32 FailsOnLine(const char* text) {
	HandScript script;
	ScriptParseError error;
	if (ParseHandScript(text, script, error)) {
		return 0;
	}
	return error.line;
}

void TestParseCommands() {
	std::printf("Reading every command\n");
	HandScript s;
	const char* text =
		"# a comment line\r\n"
		"\r\n"
		"wait 1.5   # trailing comment\r\n"
		"right pos 0.1 -0.2 -0.3\r\n"
		"left rot 10 20 30\n"
		"right trigger 1\n"
		"left grip 0.8\n"
		"right stick 0 1\n"
		"right a 1\n"
		"left b 1\n"
		"right click 1\n"
		"right trackpad 1\n"
		"left present 0\n"
		"right curl 0.9\n"
		"head yaw 45\n"
		"head pitch -10\n"
		"head pos 0 1.7 0\n"
		"head real\n"
		"mark arc_shown\n"
		"dump\n"
		"mirror\n"
		"log the arc is up\n"
		"expect Teleport: arrived\n"
		"reject WARNING\n"
		"ini Hands Enabled=1\n"
		"console SetHotKeyItem 3 {weapon}\n"
		"console-at arc_shown prid {near}\n"
		"count 1 Hands: jump sent\n";
	Check(Parses(text, s), "a script with every command reads");
	Check(s.steps.size() == 20, "twenty steps; comments, blank lines, expect and reject are not steps");
	Check(s.steps[0].op == ScriptOp::Wait && Near(s.steps[0].values[0], 1.5f) && s.steps[0].line == 3,
	      "wait with its seconds and its line number");
	Check(s.steps[1].op == ScriptOp::Hand && s.steps[1].right && s.steps[1].hand == HandField::Pos &&
	          Near(s.steps[1].values[1], -0.2f),
	      "right pos");
	Check(!s.steps[2].right && s.steps[2].hand == HandField::Rot && Near(s.steps[2].values[2], 30.0f),
	      "left rot");
	Check(s.steps[5].hand == HandField::Stick && Near(s.steps[5].values[1], 1.0f), "stick x y");
	Check(s.steps[11].hand == HandField::Curl, "curl");
	Check(s.steps[12].op == ScriptOp::Head && s.steps[12].head == HeadField::Yaw, "head yaw");
	Check(s.steps[14].head == HeadField::Pos && Near(s.steps[14].values[1], 1.7f), "head pos");
	Check(s.steps[15].head == HeadField::Real, "head real");
	Check(s.steps[16].op == ScriptOp::Mark && s.steps[16].text == "arc_shown", "mark with its name");
	Check(s.steps[17].op == ScriptOp::Dump && s.steps[18].op == ScriptOp::Mirror, "dump, mirror");
	Check(s.steps[19].op == ScriptOp::Log && s.steps[19].text == "the arc is up",
	      "log keeps its text with spaces");
	Check(s.expects.size() == 1 && s.expects[0] == "Teleport: arrived", "expect, text kept whole");
	Check(s.rejects.size() == 1 && s.rejects[0] == "WARNING", "reject");
	Check(s.ini.size() == 1 && s.ini[0] == "Hands Enabled=1", "ini, for the runner, not a step");
	Check(s.counts.size() == 1 && s.counts[0] == "1 Hands: jump sent", "count, for the runner");
	Check(s.console.size() == 1 && s.console[0] == "SetHotKeyItem 3 {weapon}",
	      "console, for the runner, with its spaces");
	Check(s.consoleAt.size() == 1 && s.consoleAt[0] == "arc_shown prid {near}",
	      "console-at, for the runner: the mark and the command");

	HandScript empty;
	Check(Parses("", empty) && empty.steps.empty(), "an empty script reads, with nothing to do");
	Check(Parses("wait 1", empty) && empty.steps.size() == 1, "no newline at the end");
}

void TestParseErrors() {
	std::printf("Refusing what cannot be read\n");
	Check(FailsOnLine("jump\n") == 1, "an unknown command");
	Check(FailsOnLine("wait\n") == 1, "wait without seconds");
	Check(FailsOnLine("wait -1\n") == 1, "a negative wait");
	Check(FailsOnLine("wait soon\n") == 1, "a wait that is not a number");
	Check(FailsOnLine("wait 1\nright\n") == 2, "a hand without a field, on its own line");
	Check(FailsOnLine("right wave 1\n") == 1, "an unknown hand field");
	Check(FailsOnLine("right pos 1 2\n") == 1, "too few values");
	Check(FailsOnLine("right a 1 1\n") == 1, "too many values");
	Check(FailsOnLine("right stick x 1\n") == 1, "a hand value that is not a number");
	Check(FailsOnLine("head\n") == 1, "head without a field");
	Check(FailsOnLine("head roll 3\n") == 1, "an unknown head field");
	Check(FailsOnLine("head real 1\n") == 1, "head real takes nothing");
	Check(FailsOnLine("head yaw left\n") == 1, "a head value that is not a number");
	Check(FailsOnLine("mark\n") == 1, "mark without a name");
	Check(FailsOnLine("mark two words\n") == 1, "a mark name with a space");
	Check(FailsOnLine("dump now\n") == 1, "dump takes nothing");
	Check(FailsOnLine("log\n") == 1, "log without text");
	Check(FailsOnLine("expect   \n") == 1, "expect without text");
	Check(FailsOnLine("console\n") == 1, "console without a command");
	Check(FailsOnLine("console-at mark\n") == 1, "console-at without a command");
	Check(FailsOnLine("key 1\n") == 1, "key without down or up");
	Check(FailsOnLine("key 1 2\n") == 1, "key with neither 0 nor 1");
	Check(FailsOnLine("key a 1\n") == 1, "a lower-case key name");
	Check(FailsOnLine("key 0xZZ 1\n") == 1, "a key code that is not hex");
	Check(FailsOnLine("key 0x0 1\n") == 1, "key code zero");
	Check(FailsOnLine("key F1 1\n") == 1, "a key name longer than one letter");
	Check(FailsOnLine("count 1\n") == 1, "count without text");
	Check(FailsOnLine("count x jump\n") == 1, "count without a number");
	Check(FailsOnLine("count -1 jump\n") == 1, "a negative count");
	Check(FailsOnLine("action\n") == 1, "action without a name");
	Check(FailsOnLine("action a b\n") == 1, "action with two names");
	Check(FailsOnLine("ini Hands\n") == 1, "ini without Key=Value");
	Check(FailsOnLine("ini Hands Enabled\n") == 1, "ini without the equals sign");
	Check(FailsOnLine("ini Hands =1\n") == 1, "ini without a key");
	HandScript s;
	ScriptParseError error;
	Check(!ParseHandScript("wait 1\nbogus\nwait 2\n", s, error) && s.steps.empty(),
	      "a failed read keeps nothing of the script");
	Check(error.message == "unknown command", "and says why");
}

void TestStepping() {
	std::printf("Walking the script\n");
	HandScript s;
	Parses("right a 1\nmark down\nwait 0.2\nright a 0\nlog up\nwait 0.1\ndump\nmirror\n", s);
	HandScriptRun run;
	HandScriptEvents ev;
	StepHandScript(s, run, 0.011f, ev);
	Check(run.pose.right.a && ev.said.size() == 1 && ev.said[0]->text == "down",
	      "the first frame runs up to the first wait");
	Check(!ev.finished && !run.finished, "not finished while waiting");
	StepHandScript(s, run, 0.1f, ev);
	Check(run.pose.right.a && ev.said.empty(), "the state stands during the wait");
	StepHandScript(s, run, 0.1f, ev);
	Check(!run.pose.right.a && ev.said.size() == 1 && ev.said[0]->text == "up",
	      "the wait over, the next commands run");
	// 0.211 run, the second wait ends at 0.3: the leftover carries.
	StepHandScript(s, run, 0.08f, ev);
	Check(!ev.dump, "the carried leftover keeps the second wait honest");
	StepHandScript(s, run, 0.02f, ev);
	Check(ev.dump && ev.mirror && ev.finished && run.finished, "dump, mirror, and the end");
	StepHandScript(s, run, 1.0f, ev);
	Check(!ev.finished && !ev.dump, "a finished script does nothing more");

	HandScript keyed;
	Check(Parses("key 1 1\nkey 0x70 0\n", keyed) && keyed.steps.size() == 2, "key commands read");
	HandScriptRun k;
	StepHandScript(keyed, k, 0.0f, ev);
	Check(ev.keys.size() == 2 && Near(ev.keys[0]->values[0], static_cast<float>('1')) &&
	          Near(ev.keys[0]->values[1], 1.0f) && Near(ev.keys[1]->values[0], 112.0f) &&
	          Near(ev.keys[1]->values[1], 0.0f),
	      "a digit as its own code, 0x70 as F1; down and up");

	HandScript acted;
	Parses("action holster_fit\nwait 1\n", acted);
	HandScriptRun a;
	StepHandScript(acted, a, 0.0f, ev);
	Check(ev.actions.size() == 1 && ev.actions[0]->text == "holster_fit" && ev.said.empty(),
	      "an action is handed out, not said");

	HandScript marked;
	Parses("right a 1\nmark pressed\nright a 0\n", marked);
	HandScriptRun m;
	StepHandScript(marked, m, 0.0f, ev);
	Check(m.pose.right.a && ev.said.size() == 1 && !ev.finished,
	      "a mark ends the step: its frame shows A still down");
	StepHandScript(marked, m, 0.011f, ev);
	Check(!m.pose.right.a && ev.finished, "the commands after it run on the next frame");

	Check(HandScriptRun{}.pose.head.synthetic, "the head is synthetic until a script says real");
	HandScript heads;
	Parses("head real\nhead yaw 30\nhead pitch -5\nhead pos 1 2 3\nwait 1\nhead real\n", heads);
	HandScriptRun h;
	StepHandScript(heads, h, 0.0f, ev);
	Check(h.pose.head.synthetic && Near(h.pose.head.yaw, 30.0f) && Near(h.pose.head.pitch, -5.0f) &&
	          Near(h.pose.head.position.z, 3.0f),
	      "head yaw, pitch and pos make the head synthetic");
	StepHandScript(heads, h, 1.0f, ev);
	Check(!h.pose.head.synthetic, "head real gives the headset back");

	HandScript fields;
	Parses("left trigger 0.7\nleft grip 0.6\nleft stick -1 0.5\nleft b 1\nleft click 1\n"
	       "left trackpad 1\nleft present 0\nleft curl 1\nleft rot 1 2 3\nleft pos 1 2 3\n",
	       fields);
	HandScriptRun f;
	StepHandScript(fields, f, 0.0f, ev);
	const ScriptHand& l = f.pose.left;
	Check(Near(l.trigger, 0.7f) && Near(l.grip, 0.6f) && Near(l.stickX, -1.0f) && l.b && l.click &&
	          l.trackpad && !l.present && Near(l.curl, 1.0f) && Near(l.roll, 3.0f) &&
	          Near(l.position.y, 2.0f),
	      "every hand field lands on the left hand");
	Check(f.pose.right.present && !f.pose.right.b, "and not on the right");
	Check(ev.finished, "a script without a wait finishes in its first frame");
}

void TestPose() {
	std::printf("The controller a scripted hand becomes\n");
	ScriptHand hand;
	hand.position = NiPoint3{0.2f, -0.4f, -0.3f};
	hand.a = true;
	hand.grip = 0.9f;
	hand.trigger = 0.3f;
	hand.stickY = 1.0f;
	const NiPoint3 headPos{1.0f, 1.6f, 2.0f};
	vr::HandPose p = ScriptedHandPose(hand, vr::Quaternion::Identity(), headPos);
	Check(p.valid, "tracked");
	Check(Near(p.position.x, 1.2f) && Near(p.position.y, 1.2f) && Near(p.position.z, 1.7f),
	      "placed from the head");
	Check(vr::ButtonADown(p.buttonsPressed) && vr::GripDown(p.buttonsPressed), "A and grip down");
	Check(!vr::ButtonBDown(p.buttonsPressed) && !vr::StickClickDown(p.buttonsPressed) &&
	          !vr::TrackpadClickDown(p.buttonsPressed),
	      "B, the stick click and the trackpad up");
	Check(!vr::ButtonDown(p.buttonsPressed, vr::openvr::kButtonTrigger) && Near(p.trigger, 0.3f),
	      "a light trigger is an axis, not the button");
	Check(Near(p.thumbY, 1.0f) && Near(p.gripForce, 0.9f), "stick and grip axes");
	Check(p.actionInput && p.actionActiveMask == 0x1F, "as the action path, everything bound");
	Check(p.curlValid && Near(p.curl[0], 0.0f) && Near(p.curl[4], 0.0f),
	      "a skeleton with every finger open");
	hand.curl = 0.9f;
	p = ScriptedHandPose(hand, vr::Quaternion::Identity(), headPos);
	Check(Near(p.curl[1], 0.9f) && Near(p.curl[4], 0.9f), "curl closes every finger");
	hand.curl = 0.0f;

	hand.a = false;
	hand.grip = 0.0f;
	hand.b = true;
	hand.click = true;
	hand.trackpad = true;
	hand.trigger = 1.0f;
	p = ScriptedHandPose(hand, vr::Quaternion::Identity(), headPos);
	Check(vr::ButtonBDown(p.buttonsPressed) && vr::StickClickDown(p.buttonsPressed) &&
	          vr::TrackpadClickDown(p.buttonsPressed) &&
	          vr::ButtonDown(p.buttonsPressed, vr::openvr::kButtonTrigger) &&
	          !vr::GripDown(p.buttonsPressed),
	      "B, click, trackpad, a full trigger; the grip let go");

	// A head turned 90 degrees left (about up): a hand 0.3 m ahead is then
	// 0.3 m to the left in tracking space.
	ScriptHand ahead;
	ahead.position = NiPoint3{0.0f, 0.0f, -0.3f};
	const vr::Quaternion left90 = vr::FromAxisAngle(0.0f, 1.0f, 0.0f, 90.0f);
	p = ScriptedHandPose(ahead, left90, NiPoint3{0.0f, 0.0f, 0.0f});
	Check(Near(p.position.x, -0.3f) && Near(p.position.z, 0.0f), "turned with the head's heading");
	// The same head also tipped down 60 degrees: the hand stays level.
	const vr::Quaternion tipped = left90 * vr::FromAxisAngle(1.0f, 0.0f, 0.0f, -60.0f);
	p = ScriptedHandPose(ahead, tipped, NiPoint3{0.0f, 0.0f, 0.0f});
	Check(Near(p.position.x, -0.3f) && Near(p.position.y, 0.0f), "but not with its pitch");
	const vr::Quaternion heading = HeadingOf(tipped);
	Check(Near(std::fabs(heading.w), std::fabs(left90.w)) && Near(std::fabs(heading.y), std::fabs(left90.y)),
	      "the heading of a tipped head is its yaw alone");

	ScriptHand gone;
	gone.present = false;
	gone.a = true;
	p = ScriptedHandPose(gone, vr::Quaternion::Identity(), headPos);
	Check(!p.valid && p.buttonsPressed == 0, "a controller not present is not tracked and presses nothing");

	ScriptHead head;
	head.yaw = 90.0f;
	const NiPoint3 f = vr::ToMatrix(ScriptHeadOrientation(head)) * NiPoint3{0.0f, 0.0f, -1.0f};
	Check(Near(f.x, -1.0f) && Near(f.z, 0.0f), "a synthetic head's yaw turns it left");
}

}  // namespace

int main() {
	TestParseCommands();
	TestParseErrors();
	TestStepping();
	TestPose();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
