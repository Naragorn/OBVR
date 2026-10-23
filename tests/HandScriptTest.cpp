// Checks the hand script: reading it, walking it on the frames' clock, and
// the controller state a scripted hand becomes. And the runner's work OBVR
// does itself (test/HandScriptConsole.h): every flow of the console lines it
// hands to the game one a frame, and of loading the run's save from the main
// menu.

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "test/HandScript.h"
#include "test/HandScriptConsole.h"
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

// "start main-menu": the script starts on the main menu, no save loaded.
void TestStartLine() {
	std::printf("Where the script starts\n");
	HandScript script;
	Check(Parses("wait 1\n", script) && !script.startOnMainMenu, "the default starts in the world");
	Check(Parses("start main-menu\nwait 1\n", script) && script.startOnMainMenu, "start main-menu");
	Check(Parses("start world\nwait 1\n", script) && !script.startOnMainMenu, "start world is the default said");
	Check(FailsOnLine("start\n") == 1 && FailsOnLine("start loading\n") == 1, "start takes main-menu or world");
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
		"count 1 Hands: jump sent\n"
		"background-at arc_shown\n"
		"moved start arc_shown 60\n"
		"still a b 10.5\n"
		"turned a b 15\n";
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
	Check(s.backgroundAt.size() == 1 && s.backgroundAt[0] == "arc_shown",
	      "background-at, for the runner: the mark, not a step");
	Check(s.walkChecks.size() == 3 && s.walkChecks[0] == "moved start arc_shown 60" &&
	          s.walkChecks[1] == "still a b 10.5" && s.walkChecks[2] == "turned a b 15",
	      "moved, still and turned, for the runner, not steps");

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
	Check(FailsOnLine("head tilt 3\n") == 1, "an unknown head field");
	Check(FailsOnLine("head real 1\n") == 1, "head real takes nothing");
	Check(FailsOnLine("head face\n") == 1, "head face takes on or off");
	{
		HandScript face;
		ScriptParseError error;
		Check(ParseHandScript("head face 1\nwait 1\nhead yaw 10\n", face, error) && face.steps[0].op == ScriptOp::Head &&
		          face.steps[0].head == HeadField::Face && face.steps[0].values[0] == 1.0f,
		      "head face 1 parses");
		ScriptHead head;
		ApplyHeadField(head, face.steps[0]);
		Check(head.faceFighter && head.synthetic, "applied: the head faces the fighter");
		ApplyHeadField(head, face.steps[2]);
		Check(!head.faceFighter && head.yaw == 10.0f, "a yaw set after it ends the facing");
	}
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
	Check(FailsOnLine("background-at\n") == 1, "background-at without a mark");
	Check(FailsOnLine("background-at a b\n") == 1, "background-at with two marks");
	Check(FailsOnLine("moved a b\n") == 1, "moved without a distance");
	Check(FailsOnLine("still a b far\n") == 1, "still with a distance that is not a number");
	Check(FailsOnLine("turned a b -5\n") == 1, "a negative turn");
	Check(FailsOnLine("moved a b 5 6\n") == 1, "moved with a fourth word");
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
	Parses("head real\nhead yaw 30\nhead pitch -5\nhead roll 15\nhead pos 1 2 3\nwait 1\nhead real\n", heads);
	HandScriptRun h;
	StepHandScript(heads, h, 0.0f, ev);
	Check(h.pose.head.synthetic && Near(h.pose.head.yaw, 30.0f) && Near(h.pose.head.pitch, -5.0f) &&
	          Near(h.pose.head.roll, 15.0f) && Near(h.pose.head.position.z, 3.0f),
	      "head yaw, pitch, roll and pos make the head synthetic");
	StepHandScript(heads, h, 1.0f, ev);
	Check(!h.pose.head.synthetic, "head real gives the headset back");

	{
		// The roll tilts the head about its view: its up leans towards its left.
		ScriptHead tilted;
		tilted.roll = 30.0f;
		const NiPoint3 up = obvr::vr::ToMatrix(ScriptHeadOrientation(tilted)) * NiPoint3{0.0f, 1.0f, 0.0f};
		const NiPoint3 ahead = obvr::vr::ToMatrix(ScriptHeadOrientation(tilted)) * NiPoint3{0.0f, 0.0f, -1.0f};
		Check(Near(up.y, 0.866f, 0.01f) && Near(std::fabs(up.x), 0.5f, 0.01f) && Near(ahead.z, -1.0f, 0.001f),
		      "a rolled head tilts its up by the roll and still looks ahead");
	}
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

	HandScript thumbs;
	Parses("right thumb 0.2 0.5 0.8\nleft thumb -1 0 0\n", thumbs);
	HandScriptRun t;
	StepHandScript(thumbs, t, 0.0f, ev);
	Check(t.pose.right.thumbJoints && Near(t.pose.right.thumb[0], 0.2f) && Near(t.pose.right.thumb[2], 0.8f),
	      "thumb gives the joints, base to tip");
	Check(!t.pose.left.thumbJoints, "a negative first value: back to the curl alone");

	HandScript littles;
	Parses("left curl 1\nleft little 0\nright little -1\n", littles);
	HandScriptRun lr;
	StepHandScript(littles, lr, 0.0f, ev);
	Check(Near(lr.pose.left.curl, 1.0f) && Near(lr.pose.left.little, 0.0f), "little gives the little finger its own curl");
	Check(lr.pose.right.little < 0.0f, "a negative one: as the curl again");
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
	Check(Near(p.thumbY, 1.0f) && Near(p.gripForce, 0.9f) && Near(p.gripPull, 0.9f),
	      "stick and grip axes - the script's grip is the pull and the force alike");
	Check(p.actionInput && p.actionActiveMask == 0x1FF, "as the action path, all nine actions bound");
	Check(p.curlValid && Near(p.curl[0], 0.0f) && Near(p.curl[4], 0.0f),
	      "a skeleton with every finger open");
	hand.curl = 0.9f;
	p = ScriptedHandPose(hand, vr::Quaternion::Identity(), headPos);
	Check(Near(p.curl[1], 0.9f) && Near(p.curl[4], 0.9f), "curl closes every finger");
	hand.little = 0.0f;
	p = ScriptedHandPose(hand, vr::Quaternion::Identity(), headPos);
	Check(Near(p.curl[3], 0.9f) && Near(p.curl[4], 0.0f), "a little finger of its own stays open while the rest close");
	hand.little = -1.0f;
	Check(!p.thumbValid, "no thumb joints unless the script gives them");
	hand.thumbJoints = true;
	hand.thumb[0] = 0.3f;
	hand.thumb[1] = 0.6f;
	hand.thumb[2] = 0.1f;
	p = ScriptedHandPose(hand, vr::Quaternion::Identity(), headPos);
	Check(p.thumbValid && Near(p.thumb[0], 0.3f) && Near(p.thumb[1], 0.6f) && Near(p.thumb[2], 0.1f),
	      "the script's thumb joints, as the skeleton would give them");
	hand.thumbJoints = false;
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

	// The eyes swing about the neck (ScriptHeadPosition): level, the head's
	// position is the eyes; nodded down, the eyes go down and a little
	// forward; turned, sideways - the neck 8 cm behind them stays put.
	ScriptHead level;
	level.position = NiPoint3{0.1f, 1.6f, 0.2f};
	const NiPoint3 eyesLevel = ScriptHeadPosition(level);
	Check(Near(eyesLevel.x, 0.1f) && Near(eyesLevel.y, 1.6f) && Near(eyesLevel.z, 0.2f), "the head level: its position is the eyes");
	ScriptHead down = level;
	down.pitch = -45.0f;
	const NiPoint3 eyesDown = ScriptHeadPosition(down);
	Check(eyesDown.y < 1.6f - 0.07f && eyesDown.y > 1.6f - 0.09f, "nodded 45 degrees down: the eyes about 8 cm lower");
	Check(eyesDown.z < 0.2f - 0.02f && eyesDown.z > 0.2f - 0.04f, "and about 3 cm further forward");
	Check(Near(eyesDown.x, 0.1f), "not sideways");
	ScriptHead straightDown = level;
	straightDown.pitch = -90.0f;
	const NiPoint3 eyesStraightDown = ScriptHeadPosition(straightDown);
	Check(eyesStraightDown.y < 1.6f - 0.15f, "looking straight down: the eyes 15 cm lower, below the neck's level");
	ScriptHead turned = level;
	turned.yaw = 90.0f;
	const NiPoint3 eyesTurned = ScriptHeadPosition(turned);
	Check(eyesTurned.x < 0.1f - 0.07f && eyesTurned.z > 0.2f + 0.07f && Near(eyesTurned.y, 1.6f),
	      "turned left: the eyes swung left and back about the neck, their height kept");
	// The neck itself stays put: the eyes less their offset, turned with the head, is the same point.
	const NiPoint3 neckOffset{0.0f, kScriptNeckToEyeUpMetres, -kScriptNeckToEyeForwardMetres};
	const NiPoint3 neckLevel = eyesLevel - vr::ToMatrix(ScriptHeadOrientation(level)) * neckOffset;
	const NiPoint3 neckDown = eyesDown - vr::ToMatrix(ScriptHeadOrientation(down)) * neckOffset;
	const NiPoint3 neckTurned = eyesTurned - vr::ToMatrix(ScriptHeadOrientation(turned)) * neckOffset;
	Check(Near(neckLevel.x, neckDown.x) && Near(neckLevel.y, neckDown.y) && Near(neckLevel.z, neckDown.z) &&
	          Near(neckLevel.x, neckTurned.x) && Near(neckLevel.y, neckTurned.y) && Near(neckLevel.z, neckTurned.z),
	      "the neck under every pose: one point");
}

void TestConsoleLines() {
	std::printf("The console lines OBVR runs itself\n");
	Check(ExpandScriptConsoleLine("player.equipitem {weapon} 1", 0x00000C0D, 0, 0) == "player.equipitem 00000C0D 1",
	      "{weapon}: the equipped weapon's form ID, eight hex digits");
	Check(ExpandScriptConsoleLine("{near}.activate player", 0, 0xFF000A3B, 0) == "FF000A3B.activate player",
	      "{near}: the item nearest the eyes");
	Check(ExpandScriptConsoleLine("{near} {near} {weapon}", 0xA, 0xB, 0) == "0000000B 0000000B 0000000A",
	      "every occurrence replaced");
	Check(ExpandScriptConsoleLine("tgm", 0xA, 0xB, 0) == "tgm", "a line without either: as written");
	Check(ExpandScriptConsoleLine("{actor}.resurrect 1", 0, 0, 0xFF000A3C) == "FF000A3C.resurrect 1",
	      "{actor}: the actor nearest the player");
	Check(ExpandScriptConsoleLine("{actor} {near}", 0, 0xB, 0) == "00000000 0000000B",
	      "{actor} with nobody loaded: zero, eight digits");

	HandScript script;
	Check(Parses("console player.additem 00000C0D 1\n"
	             "console player.equipitem {weapon} 1\n"
	             "console-at grabbed {near}.disable\n"
	             "console-at other tgm\n"
	             "console-at grabbed player.drop {weapon} 1\n"
	             "wait 1\nmark grabbed\n",
	             script),
	      "a script with console and console-at lines parses");

	ScriptConsoleFeed feed;
	QueueStartConsoleLines(script, feed);
	Check(feed.lines.size() == 2 && feed.lines[0] == "player.additem 00000C0D 1", "the start: the console lines, in order");
	std::vector<std::string> ran;
	auto take = [&ran](const std::string& line) {
		ran.push_back(line);
		return true;
	};
	const float frame = 1.0f / 60.0f;
	ScriptConsoleStep step;
	for (int i = 0; i < 3; ++i) {
		step = StepScriptConsoleFeed(feed, 1.0f, 0x12345678, 0, 0, take);
	}
	Check(!step.handed && !step.dropped && ran.empty(), "the start's lines wait while the world settles: three seconds, none");
	step = StepScriptConsoleFeed(feed, 1.0f, 0x12345678, 0, 0, take);
	Check(step.handed && step.line == "player.additem 00000C0D 1" && ran.size() == 1,
	      "at the settle time the first line, one in a frame");
	step = StepScriptConsoleFeed(feed, frame, 0x12345678, 0, 0, take);
	Check(step.handed && step.line == "player.equipitem 12345678 1", "the next frame the next, {weapon} filled in");
	step = StepScriptConsoleFeed(feed, frame, 0x12345678, 0, 0, take);
	Check(!step.handed && !step.dropped && ran.size() == 2, "nothing left: nothing handed over");

	Check(QueueConsoleLinesAt(script, "grabbed", feed) == 2, "the mark's console-at lines queued, the other mark's not");
	step = StepScriptConsoleFeed(feed, frame, 0x12345678, 0x00ABCDEF, 0, take);
	Check(!step.handed && ran.size() == 2, "held the mark's own frame: the items line comes later in it");
	step = StepScriptConsoleFeed(feed, frame, 0x12345678, 0x00ABCDEF, 0, take);
	Check(!step.handed, "and the frame after, for the items line to be written");
	step = StepScriptConsoleFeed(feed, frame, 0x12345678, 0x00ABCDEF, 0, take);
	Check(step.handed && step.line == "00ABCDEF.disable", "then {near} from the mark's items line - no settle time at a mark");
	step = StepScriptConsoleFeed(feed, frame, 0x12345678, 0x00ABCDEF, 0, take);
	Check(step.handed && step.line == "player.drop 12345678 1", "then the mark's second line");
	Check(QueueConsoleLinesAt(script, "nowhere", feed) == 0, "a mark without console-at lines queues none");
	HandScript marksOnly;
	Parses("console-at a tgm\nwait 1\nmark a\n", marksOnly);
	ScriptConsoleFeed noStart;
	QueueStartConsoleLines(marksOnly, noStart);
	Check(noStart.lines.empty() && noStart.holdSeconds == 0.0f, "no start lines: no settle time for the marks' lines");

	ScriptConsoleFeed busy;
	busy.lines.push_back("tgm");
	UInt32 asked = 0;
	auto refuse = [&asked](const std::string&) {
		++asked;
		return false;
	};
	step = StepScriptConsoleFeed(busy, frame, 0, 0, 0, refuse);
	Check(!step.handed && !step.dropped && busy.next == 0 && asked == 1, "refused (the console's queue full): kept");
	for (UInt32 i = 1; i < kScriptConsoleRefusalsMost; ++i) {
		step = StepScriptConsoleFeed(busy, frame, 0, 0, 0, refuse);
	}
	Check(step.dropped && !step.handed && busy.next == 1 && asked == kScriptConsoleRefusalsMost,
	      "refused frame after frame: given up after the limit");

	ScriptConsoleFeed tooLong;
	tooLong.lines.push_back(std::string(kScriptConsoleLineMost + 1, 'x'));
	tooLong.lines.push_back("tgm");
	ran.clear();
	step = StepScriptConsoleFeed(tooLong, frame, 0, 0, 0, take);
	Check(step.dropped && !step.handed && ran.empty() && tooLong.next == 1,
	      "longer than the console takes: given up without asking");
	step = StepScriptConsoleFeed(tooLong, frame, 0, 0, 0, take);
	Check(step.handed && step.line == "tgm", "and the next line goes on");
	ScriptConsoleFeed fits;
	fits.lines.push_back(std::string(kScriptConsoleLineMost, 'x'));
	step = StepScriptConsoleFeed(fits, frame, 0, 0, 0, take);
	Check(step.handed, "exactly as long as the console takes: handed over");
}

void TestSaveLoad() {
	std::printf("The run's save, loaded from the main menu\n");
	ScriptSaveLoad s;
	Check(!StepScriptSaveLoad(s, false, true, 5.0f), "no save named: never");
	Check(!StepScriptSaveLoad(s, true, false, 5.0f) && s.mainMenuSeconds == 0.0f, "not in the main menu: never");
	Check(!StepScriptSaveLoad(s, true, true, 1.0f), "the main menu up a second: not yet");
	Check(StepScriptSaveLoad(s, true, true, 1.0f) && s.asked == 1, "up two seconds: asked for");
	Check(!StepScriptSaveLoad(s, true, true, 1.0f), "asked once: not again at once");
	Check(!StepScriptSaveLoad(s, true, false, 0.1f) && s.mainMenuSeconds == 0.0f,
	      "the main menu gone (the load under way): the clock starts over");
	ScriptSaveLoad stuck;
	StepScriptSaveLoad(stuck, true, true, kScriptLoadAfterSeconds);
	Check(!StepScriptSaveLoad(stuck, true, true, kScriptLoadRetrySeconds - 0.5f), "still in the main menu: not yet again");
	Check(StepScriptSaveLoad(stuck, true, true, 1.0f) && stuck.asked == 2, "still there after the retry time: asked again");
	StepScriptSaveLoad(stuck, true, true, kScriptLoadRetrySeconds);
	Check(stuck.asked == kScriptLoadAsksMost, "asked a third time");
	Check(!StepScriptSaveLoad(stuck, true, true, 100.0f), "after the last ask: never again");

	// console-menu lines: handed to the main menu's console before the load.
	HandScript menu;
	ScriptParseError err;
	Check(ParseHandScript("console-menu SetRaceScale HighElf 0 1.1\nconsole-menu SetRaceScale HighElf 1 1.1\nwait 1\n", menu,
	                      err) &&
	          menu.consoleMenu.size() == 2 && menu.consoleMenu[1] == "SetRaceScale HighElf 1 1.1",
	      "console-menu lines parse, for the main menu");
	Check(FailsOnLine("console-menu\n") == 1, "console-menu without a command");
	ScriptSaveLoad menuLoad;
	Check(ScriptMenuLinesDue(menuLoad, true, true, 0, 6) == 0, "menu lines: none before the menu has stood two seconds");
	Check(!StepScriptSaveLoad(menuLoad, true, true, 2.5f, true), "with menu lines left the load waits");
	Check(ScriptMenuLinesDue(menuLoad, true, true, 0, 6) == 4, "then four a frame");
	Check(ScriptMenuLinesDue(menuLoad, true, true, 4, 6) == 2, "the rest in the next");
	Check(ScriptMenuLinesDue(menuLoad, true, true, 6, 6) == 0, "all handed: none");
	Check(ScriptMenuLinesDue(menuLoad, false, true, 0, 6) == 0 && ScriptMenuLinesDue(menuLoad, true, false, 0, 6) == 0,
	      "no save, or no main menu: none");
	Check(StepScriptSaveLoad(menuLoad, true, true, 0.1f, false) && menuLoad.asked == 1,
	      "all handed: the load asked for");
}

}  // namespace

int main() {
	TestStartLine();
	TestParseCommands();
	TestParseErrors();
	TestStepping();
	TestPose();
	TestConsoleLines();
	TestSaveLoad();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
