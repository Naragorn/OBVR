#pragma once

// The hand script: controllers played from a text file instead of held, so a
// Full VR feature can be run in the game with nobody in the headset
// ([Debug] HandScript, tools/hand-script-run.ps1). This header is the pure
// part - the parser, the clock that walks the script, and the controller
// state a step leaves behind - and is covered by hand_script_test.
//
// A script is one command per line; '#' starts a comment.
//
//   wait <seconds>                   let the state stand for that long
//   right|left pos <x> <y> <z>       the hand, metres from the head: x right,
//                                    y up, z back (OpenVR's axes), in the
//                                    head's heading (its yaw only)
//   right|left rot <pitch> <yaw> <roll>   degrees, from pointing ahead
//   right|left trigger|grip <0..1>
//   right|left stick <x> <y>
//   right|left a|b|click|trackpad <0|1>
//   right|left present <0|1>         whether the controller is tracked
//   right|left curl <0..1>           all fingers' curl (the fist)
//   head yaw|pitch <degrees>         a synthetic head, turned
//   head pos <x> <y> <z>             and placed, in tracking space
//   head real                        the headset's own pose. The default is a
//                                    synthetic head 1.6 m up looking ahead: a
//                                    sleeping headset gives no valid pose
//                                    (2026-09-27, "head 0" in the state line)
//   mark <name>                      a log line the runner takes a
//                                    screenshot on
//   dump                             SteamVR's CompositorDumpImages
//   mirror                           SteamVR's headset view window
//   log <text>                       a line in OBVR.log
//   ini <Section> <Key>=<Value>      for the runner: written to OBVR-test.ini
//   console <command>                for the runner: typed into the console once
//                                    the script runs; {weapon} is the equipped
//                                    weapon's form ID
//   expect <text> / reject <text>    read by the runner, not by OBVR: the
//                                    log must (must not) contain the text
//
// The script's time runs on the frames' own time, so a wait is as long in a
// slow frame as in a fast one.

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::test {

struct ScriptHand {
	bool present = true;
	NiPoint3 position{0.0f, 0.0f, 0.0f};
	float pitch = 0.0f;
	float yaw = 0.0f;
	float roll = 0.0f;
	float trigger = 0.0f;
	float grip = 0.0f;
	float stickX = 0.0f;
	float stickY = 0.0f;
	float curl = 0.0f;
	bool a = false;
	bool b = false;
	bool click = false;
	bool trackpad = false;
};

struct ScriptHead {
	bool synthetic = true;
	float yaw = 0.0f;
	float pitch = 0.0f;
	NiPoint3 position{0.0f, 1.6f, 0.0f};
};

// Where the hands start: low in front, as they hang holding controllers.
struct ScriptPose {
	ScriptHand right;
	ScriptHand left;
	ScriptHead head;
	ScriptPose() {
		right.position = NiPoint3{0.20f, -0.45f, -0.30f};
		left.position = NiPoint3{-0.20f, -0.45f, -0.30f};
	}
};

enum class ScriptOp : UInt8 {
	Wait,
	Hand,
	Head,
	Mark,
	Dump,
	Mirror,
	Log,
};

enum class HandField : UInt8 {
	Pos,
	Rot,
	Trigger,
	Grip,
	Stick,
	A,
	B,
	Click,
	Trackpad,
	Present,
	Curl,
};

enum class HeadField : UInt8 {
	Yaw,
	Pitch,
	Pos,
	Real,
};

struct ScriptStep {
	ScriptOp op = ScriptOp::Wait;
	bool right = true;
	HandField hand = HandField::Pos;
	HeadField head = HeadField::Yaw;
	float values[3] = {0.0f, 0.0f, 0.0f};
	std::string text;
	UInt32 line = 0;
};

struct HandScript {
	std::vector<ScriptStep> steps;
	std::vector<std::string> expects;
	std::vector<std::string> rejects;
	std::vector<std::string> ini;  // "Section Key=Value", for the runner
	std::vector<std::string> console;  // console commands, for the runner
};

struct ScriptParseError {
	UInt32 line = 0;
	std::string message;
};

namespace detail {

inline std::vector<std::string> Tokens(const std::string& line) {
	std::vector<std::string> out;
	size_t i = 0;
	while (i < line.size()) {
		while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
			++i;
		}
		const size_t start = i;
		while (i < line.size() && line[i] != ' ' && line[i] != '\t') {
			++i;
		}
		if (i > start) {
			out.push_back(line.substr(start, i - start));
		}
	}
	return out;
}

inline bool Number(const std::string& text, float& out) {
	if (text.empty()) {
		return false;
	}
	char* end = nullptr;
	out = std::strtof(text.c_str(), &end);
	return end != nullptr && *end == '\0';
}

// The rest of the line after the first word, as written.
inline std::string Rest(const std::string& line, const std::string& word) {
	const size_t at = line.find(word);
	size_t i = at + word.size();
	while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
		++i;
	}
	std::string rest = line.substr(i);
	while (!rest.empty() && (rest.back() == ' ' || rest.back() == '\t' || rest.back() == '\r')) {
		rest.pop_back();
	}
	return rest;
}

struct FieldName {
	const char* name;
	HandField field;
	int count;
};

constexpr FieldName kHandFields[] = {
	{"pos", HandField::Pos, 3},         {"rot", HandField::Rot, 3},
	{"trigger", HandField::Trigger, 1}, {"grip", HandField::Grip, 1},
	{"stick", HandField::Stick, 2},     {"a", HandField::A, 1},
	{"b", HandField::B, 1},             {"click", HandField::Click, 1},
	{"trackpad", HandField::Trackpad, 1}, {"present", HandField::Present, 1},
	{"curl", HandField::Curl, 1},
};

}  // namespace detail

// Reads a whole script. On a line it cannot read it stops and says which and
// why; nothing of a half-read script is run.
inline bool ParseHandScript(const std::string& text, HandScript& out, ScriptParseError& error) {
	out = HandScript{};
	UInt32 lineNumber = 0;
	size_t pos = 0;
	while (pos <= text.size()) {
		size_t end = text.find('\n', pos);
		if (end == std::string::npos) {
			end = text.size();
		}
		std::string line = text.substr(pos, end - pos);
		pos = end + 1;
		++lineNumber;
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();  // a file saved on Windows
		}
		const size_t hash = line.find('#');
		if (hash != std::string::npos) {
			line = line.substr(0, hash);
		}
		const std::vector<std::string> t = detail::Tokens(line);
		if (t.empty()) {
			if (end == text.size()) {
				break;
			}
			continue;
		}
		const auto fail = [&](const char* message) {
			error.line = lineNumber;
			error.message = message;
			out = HandScript{};
			return false;
		};
		ScriptStep step;
		step.line = lineNumber;
		const std::string& word = t[0];
		if (word == "wait") {
			if (t.size() != 2 || !detail::Number(t[1], step.values[0]) || step.values[0] < 0.0f) {
				return fail("wait takes one duration in seconds");
			}
			step.op = ScriptOp::Wait;
		} else if (word == "right" || word == "left") {
			if (t.size() < 2) {
				return fail("a hand needs a field");
			}
			step.op = ScriptOp::Hand;
			step.right = word == "right";
			const detail::FieldName* found = nullptr;
			for (const detail::FieldName& f : detail::kHandFields) {
				if (t[1] == f.name) {
					found = &f;
				}
			}
			if (found == nullptr) {
				return fail("unknown hand field");
			}
			if (static_cast<int>(t.size()) != 2 + found->count) {
				return fail("wrong number of values for the hand field");
			}
			step.hand = found->field;
			for (int i = 0; i < found->count; ++i) {
				if (!detail::Number(t[2 + i], step.values[i])) {
					return fail("a hand value is not a number");
				}
			}
		} else if (word == "head") {
			if (t.size() < 2) {
				return fail("head needs a field");
			}
			step.op = ScriptOp::Head;
			int count = 0;
			if (t[1] == "yaw") {
				step.head = HeadField::Yaw;
				count = 1;
			} else if (t[1] == "pitch") {
				step.head = HeadField::Pitch;
				count = 1;
			} else if (t[1] == "pos") {
				step.head = HeadField::Pos;
				count = 3;
			} else if (t[1] == "real") {
				step.head = HeadField::Real;
			} else {
				return fail("unknown head field");
			}
			if (static_cast<int>(t.size()) != 2 + count) {
				return fail("wrong number of values for the head field");
			}
			for (int i = 0; i < count; ++i) {
				if (!detail::Number(t[2 + i], step.values[i])) {
					return fail("a head value is not a number");
				}
			}
		} else if (word == "mark") {
			if (t.size() != 2) {
				return fail("mark takes one name without spaces");
			}
			step.op = ScriptOp::Mark;
			step.text = t[1];
		} else if (word == "dump" || word == "mirror") {
			if (t.size() != 1) {
				return fail("dump and mirror take nothing");
			}
			step.op = word == "dump" ? ScriptOp::Dump : ScriptOp::Mirror;
		} else if (word == "console") {
			// console <command>: typed into the game's console by the runner
			// once the script has started; {weapon} becomes the equipped
			// weapon's form ID.
			const std::string rest = detail::Rest(line, word);
			if (rest.empty()) {
				return fail("console needs a command");
			}
			out.console.push_back(rest);
			continue;
		} else if (word == "ini") {
			// ini <Section> <Key>=<Value>: a line the runner writes into
			// OBVR-test.ini before the game starts.
			if (t.size() != 3 || t[2].find('=') == std::string::npos || t[2][0] == '=') {
				return fail("ini takes a section and Key=Value");
			}
			out.ini.push_back(t[1] + " " + t[2]);
			continue;
		} else if (word == "log" || word == "expect" || word == "reject") {
			const std::string rest = detail::Rest(line, word);
			if (rest.empty()) {
				return fail("log, expect and reject need text");
			}
			if (word == "expect") {
				out.expects.push_back(rest);
				continue;
			}
			if (word == "reject") {
				out.rejects.push_back(rest);
				continue;
			}
			step.op = ScriptOp::Log;
			step.text = rest;
		} else {
			return fail("unknown command");
		}
		out.steps.push_back(step);
		if (end == text.size()) {
			break;
		}
	}
	return true;
}

// Where a script is.
struct HandScriptRun {
	size_t next = 0;
	float waitLeft = 0.0f;
	bool finished = false;
	ScriptPose pose;
};

// What a step of the clock did besides the pose, for the runtime to act on.
struct HandScriptEvents {
	std::vector<const ScriptStep*> said;  // marks and log lines, in order
	bool dump = false;
	bool mirror = false;
	bool finished = false;  // the script ran out in this step
};

inline void ApplyHandField(ScriptHand& hand, const ScriptStep& s) {
	const float* v = s.values;
	switch (s.hand) {
	case HandField::Pos:
		hand.position = NiPoint3{v[0], v[1], v[2]};
		break;
	case HandField::Rot:
		hand.pitch = v[0];
		hand.yaw = v[1];
		hand.roll = v[2];
		break;
	case HandField::Trigger:
		hand.trigger = v[0];
		break;
	case HandField::Grip:
		hand.grip = v[0];
		break;
	case HandField::Stick:
		hand.stickX = v[0];
		hand.stickY = v[1];
		break;
	case HandField::A:
		hand.a = v[0] != 0.0f;
		break;
	case HandField::B:
		hand.b = v[0] != 0.0f;
		break;
	case HandField::Click:
		hand.click = v[0] != 0.0f;
		break;
	case HandField::Trackpad:
		hand.trackpad = v[0] != 0.0f;
		break;
	case HandField::Present:
		hand.present = v[0] != 0.0f;
		break;
	case HandField::Curl:
		hand.curl = v[0];
		break;
	}
}

inline void ApplyHeadField(ScriptHead& head, const ScriptStep& s) {
	switch (s.head) {
	case HeadField::Yaw:
		head.synthetic = true;
		head.yaw = s.values[0];
		break;
	case HeadField::Pitch:
		head.synthetic = true;
		head.pitch = s.values[0];
		break;
	case HeadField::Pos:
		head.synthetic = true;
		head.position = NiPoint3{s.values[0], s.values[1], s.values[2]};
		break;
	case HeadField::Real:
		head.synthetic = false;
		break;
	}
}

// Moves the script on by dt seconds: every command up to the next wait that
// has not run out is carried out, in order. A wait's leftover carries into
// the next, so the script keeps its own time across uneven frames. A mark
// ends the step: the frame it is written in shows the state the commands
// before it made, not the ones after it (the picture at a mark is of that
// frame).
inline void StepHandScript(const HandScript& script, HandScriptRun& run, float dt,
                           HandScriptEvents& events) {
	events = HandScriptEvents{};
	if (run.finished) {
		return;
	}
	run.waitLeft -= dt;
	bool marked = false;
	while (!marked && run.waitLeft <= 0.0f && run.next < script.steps.size()) {
		const ScriptStep& s = script.steps[run.next++];
		switch (s.op) {
		case ScriptOp::Wait:
			run.waitLeft += s.values[0];
			break;
		case ScriptOp::Hand:
			ApplyHandField(s.right ? run.pose.right : run.pose.left, s);
			break;
		case ScriptOp::Head:
			ApplyHeadField(run.pose.head, s);
			break;
		case ScriptOp::Mark:
			marked = true;
			events.said.push_back(&s);
			break;
		case ScriptOp::Log:
			events.said.push_back(&s);
			break;
		case ScriptOp::Dump:
			events.dump = true;
			break;
		case ScriptOp::Mirror:
			events.mirror = true;
			break;
		}
	}
	if (run.waitLeft <= 0.0f && run.next >= script.steps.size()) {
		run.finished = true;
		events.finished = true;
	}
}

}  // namespace obvr::test
