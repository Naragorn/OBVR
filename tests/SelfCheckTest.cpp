// Checks the install self-check's decisions (core/SelfCheckLogic.h): every
// check's ok, problem, hint, pending and could-not-verify, Windows against
// Proton, the readers of obse.log, dxvk.conf, Trifle.ini and a PE header,
// the log lines, and when the panel shows and what it says.

#include <cstdio>
#include <cstring>

#include "core/SelfCheckLogic.h"
#include "core/SelfCheckNotes.h"

using namespace obvr::selfcheck;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Has(const char* text, const char* word) { return std::strstr(text, word) != nullptr; }

// Everything right: the facts of a sound Windows install.
Facts Sound() {
	Facts f;
	f.oblivionVersion = kOblivion12416;
	f.obseLogRead = true;
	f.obseMajor = 22;
	f.obseMinor = 13;
	f.device = Device::Dxvk;
	f.d3d9Read = true;
	f.d3d9Gplasync = true;
	f.dxvkConfFound = true;
	f.memoryDefragOff = true;
	f.cachedWriteOnlyOn = true;
	f.menuQue = MenuQue::V16b;
	f.openVr = OpenVrDll::Ok;
	return f;
}

const Result& Of(const Result (&r)[kCheckCount], obvr::selfcheck::Check c) { return r[static_cast<UInt32>(c)]; }

State StateOf(const Facts& f, obvr::selfcheck::Check c) {
	Result r[kCheckCount];
	Evaluate(f, r);
	return Of(r, c).state;
}

const char* TextOf(const Facts& f, obvr::selfcheck::Check c) {
	static Result r[kCheckCount];
	Evaluate(f, r);
	return Of(r, c).text;
}

using C = obvr::selfcheck::Check;

void TestSound() {
	std::printf("A sound install\n");
	Result r[kCheckCount];
	Evaluate(Sound(), r);
	bool allOk = true;
	for (UInt32 i = 0; i < kCheckCount; ++i) {
		allOk = allOk && r[i].state == State::Ok && r[i].check == static_cast<C>(i);
	}
	Check(allOk, "every check ok, in Check's order");
	Check(PanelReady(r), "the panel is ready");
	Check(!PanelWanted(r, true), "no panel when nothing is wrong");
}

void TestOblivion() {
	std::printf("Oblivion\n");
	Facts f = Sound();
	f.oblivionVersion = 0;
	Check(StateOf(f, C::Oblivion) == State::Pending, "before OBSE's Query: pending");
	f.oblivionVersion = 0x010200A0;
	Check(StateOf(f, C::Oblivion) == State::Problem, "another version: problem");
	Check(Has(TextOf(f, C::Oblivion), "1.2.0.416"), "says which version");
}

void TestXobse() {
	std::printf("xOBSE\n");
	Facts f = Sound();
	f.obseLogRead = false;
	Check(StateOf(f, C::Xobse) == State::Unverified, "no obse.log line: could not verify");
	f.obseLogRead = true;
	f.obseMinor = 12;
	Check(StateOf(f, C::Xobse) == State::Problem, "22.12: too old");
	Check(Has(TextOf(f, C::Xobse), "22.12") && Has(TextOf(f, C::Xobse), "22.13"), "names the found and the needed");
	f.obseMinor = 40;
	Check(StateOf(f, C::Xobse) == State::Ok, "22.40: ok");
	f.obseMajor = 23;
	f.obseMinor = 0;
	Check(StateOf(f, C::Xobse) == State::Ok, "23.0: ok");
	f.obseMajor = 21;
	f.obseMinor = 99;
	Check(StateOf(f, C::Xobse) == State::Problem, "21.99: too old");
}

void TestDxvk() {
	std::printf("DXVK\n");
	Facts f = Sound();
	f.device = Device::NotYet;
	Check(StateOf(f, C::Dxvk) == State::Pending, "no device yet: pending");
	f.device = Device::Native;
	Check(StateOf(f, C::Dxvk) == State::Problem && Has(TextOf(f, C::Dxvk), "x32"),
	      "Windows without DXVK: problem, copy d3d9.dll");
	f.proton = true;
	Check(StateOf(f, C::Dxvk) == State::Problem && Has(TextOf(f, C::Dxvk), "PROTON_USE_WINED3D"),
	      "Proton without DXVK: problem, the wined3d switch named");
	f.device = Device::Dxvk;
	f.d3d9Read = false;
	f.d3d9Gplasync = false;
	Check(StateOf(f, C::Dxvk) == State::Ok, "Proton's own DXVK: ok, no gplasync asked");
	f.proton = false;
	Check(StateOf(f, C::Dxvk) == State::Unverified, "Windows, d3d9.dll unread: could not verify");
	f.d3d9Read = true;
	Check(StateOf(f, C::Dxvk) == State::Hint, "Windows, DXVK without gplasync: a hint");
	f.d3d9Gplasync = true;
	Check(StateOf(f, C::Dxvk) == State::Ok, "Windows, gplasync: ok");
}

void TestDxvkConf() {
	std::printf("dxvk.conf\n");
	Facts f = Sound();
	f.dxvkConfFound = false;
	Check(StateOf(f, C::DxvkConf) == State::Problem && Has(TextOf(f, C::DxvkConf), "missing"), "missing: problem");
	f.dxvkConfFromEnvironment = true;
	Check(Has(TextOf(f, C::DxvkConf), "DXVK_CONFIG_FILE"), "the environment's file is named as such");
	f.dxvkConfFromEnvironment = false;
	f.dxvkConfFound = true;
	f.memoryDefragOff = false;
	Check(StateOf(f, C::DxvkConf) == State::Problem && Has(TextOf(f, C::DxvkConf), "enableMemoryDefrag") &&
	          !Has(TextOf(f, C::DxvkConf), "cachedWriteOnly"),
	      "the defrag line lacking: named alone");
	f.memoryDefragOff = true;
	f.cachedWriteOnlyOn = false;
	Check(Has(TextOf(f, C::DxvkConf), "cachedWriteOnlyBuffers") && !Has(TextOf(f, C::DxvkConf), "Defrag"),
	      "the buffers line lacking: named alone");
	f.memoryDefragOff = false;
	Check(Has(TextOf(f, C::DxvkConf), "Defrag") && Has(TextOf(f, C::DxvkConf), "cachedWriteOnly"),
	      "both lacking: both named");
}

void TestMenuQue() {
	std::printf("MenuQue\n");
	Facts f = Sound();
	f.menuQue = MenuQue::NotYet;
	Check(StateOf(f, C::MenuQue) == State::Pending, "not looked yet: pending");
	f.menuQue = MenuQue::Missing;
	Check(StateOf(f, C::MenuQue) == State::Problem && Has(TextOf(f, C::MenuQue), "not installed"), "missing");
	f.menuQue = MenuQue::Unsupported;
	Check(StateOf(f, C::MenuQue) == State::Problem && Has(TextOf(f, C::MenuQue), "not v16b"), "another version");
}

void TestOverlay() {
	std::printf("Steam overlay\n");
	Facts f = Sound();
	f.overlayLoaded = true;
	Check(StateOf(f, C::SteamOverlay) == State::Problem, "loaded on Windows: problem");
	f.proton = true;
	Check(StateOf(f, C::SteamOverlay) == State::Problem, "loaded under Proton: problem");
	f.overlayLoaded = false;
	Check(StateOf(f, C::SteamOverlay) == State::Unverified, "not seen under Proton: could not verify");
	f.proton = false;
	Check(StateOf(f, C::SteamOverlay) == State::Ok, "not loaded on Windows: ok");
}

void TestFilesAndOpenVr() {
	std::printf("Files and openvr_api.dll\n");
	Facts f = Sound();
	f.missingFiles = 1;
	f.firstMissing = "Menus\\generic\\OBVR_Update.xml";
	Check(StateOf(f, C::Files) == State::Problem && Has(TextOf(f, C::Files), "1 OBVR file is missing") &&
	          Has(TextOf(f, C::Files), "OBVR_Update.xml"),
	      "one missing: named, singular");
	f.missingFiles = 3;
	Check(Has(TextOf(f, C::Files), "3 OBVR files are missing"), "three missing: plural");
	f.firstMissing = nullptr;
	Check(Has(TextOf(f, C::Files), "e.g. ?"), "no name: a question mark, no crash");
	f = Sound();
	f.openVr = OpenVrDll::Missing;
	Check(StateOf(f, C::OpenVr) == State::Problem && Has(TextOf(f, C::OpenVr), "missing"), "openvr missing");
	f.openVr = OpenVrDll::Not32Bit;
	Check(StateOf(f, C::OpenVr) == State::Problem && Has(TextOf(f, C::OpenVr), "32-bit"), "openvr 64-bit");
	f.openVr = OpenVrDll::Unreadable;
	Check(StateOf(f, C::OpenVr) == State::Unverified, "openvr unreadable: could not verify");
}

void TestConflictsAndTrifle() {
	std::printf("Conflicts and Trifle\n");
	Facts f = Sound();
	f.enhancedCamera = true;
	Check(StateOf(f, C::Conflicts) == State::Problem && Has(TextOf(f, C::Conflicts), "OBSE_EnhancedCamera.dll") &&
	          !Has(TextOf(f, C::Conflicts), "Reloaded"),
	      "Enhanced Camera alone");
	f.oblivionReloaded = true;
	Check(Has(TextOf(f, C::Conflicts), "OBSE_EnhancedCamera.dll") && Has(TextOf(f, C::Conflicts), "OblivionReloaded.dll"),
	      "both named");
	f.enhancedCamera = false;
	Check(Has(TextOf(f, C::Conflicts), "OblivionReloaded.dll") && !Has(TextOf(f, C::Conflicts), "Enhanced"),
	      "Oblivion Reloaded alone");
	f = Sound();
	f.trifle = Trifle::ShadowOn;
	Check(StateOf(f, C::Trifle) == State::Problem && Has(TextOf(f, C::Trifle), "EnableFirstPersonShadow=0"),
	      "Trifle's shadow on: problem");
	f.trifle = Trifle::ShadowOff;
	Check(StateOf(f, C::Trifle) == State::Ok, "Trifle's shadow off: ok");
}

void TestControllers() {
	std::printf("Controllers\n");
	Facts f = Sound();
	Check(StateOf(f, C::Controllers) == State::Ok && Has(TextOf(f, C::Controllers), "Full VR is off"),
	      "Full VR off: not checked, ok");
	f.fullVr = true;
	Check(StateOf(f, C::Controllers) == State::Pending, "Full VR, no controller named yet: pending");
	f.right = Controller::Index;
	Check(StateOf(f, C::Controllers) == State::Ok, "Index, the headset not read: ok (no guess)");
	f.hmdRead = true;
	f.hmdQuestLike = false;
	Check(StateOf(f, C::Controllers) == State::Ok, "Index with an Index-like headset: ok");
	f.hmdQuestLike = true;
	Check(StateOf(f, C::Controllers) == State::Hint && Has(TextOf(f, C::Controllers), "emulation"),
	      "Index controllers on a Quest: the emulation hint");
	f.right = Controller::Unknown;
	f.left = Controller::Index;
	Check(StateOf(f, C::Controllers) == State::Hint, "the left hand alone an Index: the hint too");
	f.left = Controller::Other;
	Check(StateOf(f, C::Controllers) == State::Ok, "Touch controllers on a Quest: ok");
	Check(HmdLooksLikeQuest("Oculus Quest 3", "Oculus"), "a Quest by its model");
	Check(HmdLooksLikeQuest("Headset", "Meta Platforms"), "a Quest by Meta's name");
	Check(!HmdLooksLikeQuest("Beyond", "Bigscreen"), "a Bigscreen Beyond is no Quest");
	Check(!HmdLooksLikeQuest("Index", "Valve") && !HmdLooksLikeQuest(nullptr, nullptr), "an Index, and nothing");
	Check(ControllerFromType("knuckles") == Controller::Index && ControllerFromType("oculus_touch") == Controller::Other &&
	          ControllerFromType("") == Controller::Unknown && ControllerFromType(nullptr) == Controller::Unknown &&
	          ControllerFromType("knuckles_x") == Controller::Other,
	      "controller types: knuckles the Index, others other, none unknown");
}

void TestObseLog() {
	std::printf("obse.log\n");
	UInt32 a = 0;
	UInt32 b = 0;
	const char* real = "OBSE: initialize (version = 22.13 010201A0)\r\noblivion root = Z:\\x\\\r\n";
	Check(ParseObseLog(real, static_cast<UInt32>(std::strlen(real)), a, b) && a == 22 && b == 13,
	      "the Linux install's first line reads 22.13");
	const char* later = "junk\nOBSE: initialize (version = 23.4 010201A0)\n";
	Check(ParseObseLog(later, static_cast<UInt32>(std::strlen(later)), a, b) && a == 23 && b == 4,
	      "found on a later line");
	const char* none = "plugin directory = Z:\\x\\\n";
	Check(!ParseObseLog(none, static_cast<UInt32>(std::strlen(none)), a, b) && a == 0, "no line: false");
	const char* broken = "OBSE: initialize (version = 22 010201A0)";
	Check(!ParseObseLog(broken, static_cast<UInt32>(std::strlen(broken)), a, b), "no minor: false");
	const char* noMinor = "OBSE: initialize (version = 22.x)";
	Check(!ParseObseLog(noMinor, static_cast<UInt32>(std::strlen(noMinor)), a, b), "a letter for the minor: false");
	Check(!ParseObseLog(nullptr, 10, a, b), "no text: false");
	const char* cut = "OBSE: initialize (version = 22";
	Check(!ParseObseLog(cut, static_cast<UInt32>(std::strlen(cut)), a, b), "cut off: false");
}

void TestDxvkConfReader() {
	std::printf("dxvk.conf reader\n");
	auto parse = [](const char* text) { return ParseDxvkConf(text, static_cast<UInt32>(std::strlen(text))); };
	DxvkConfVerdict v = parse("dxvk.enableMemoryDefrag = False\nd3d9.cachedWriteOnlyBuffers = True\n");
	Check(v.memoryDefragOff && v.cachedWriteOnlyOn, "the README's two lines");
	v = parse("dxvk.enableMemoryDefrag=false\r\n  d3d9.cachedWriteOnlyBuffers\t=  TRUE  \r\n");
	Check(v.memoryDefragOff && v.cachedWriteOnlyOn, "no spaces, CRLF, tabs, any case");
	v = parse("# dxvk.enableMemoryDefrag = False\nd3d9.cachedWriteOnlyBuffers = True # kept\n");
	Check(!v.memoryDefragOff && v.cachedWriteOnlyOn, "a commented line is no line; a trailing comment is cut");
	v = parse("dxvk.enableMemoryDefrag = True\nd3d9.cachedWriteOnlyBuffers = 1\n");
	Check(!v.memoryDefragOff && !v.cachedWriteOnlyOn, "the wrong values, and 1 is no boolean to DXVK");
	v = parse("dxvk.enableMemoryDefrag = False\ndxvk.enableMemoryDefrag = True\n");
	Check(!v.memoryDefragOff, "the last line for a key wins");
	v = parse("d3d9.samplerAnisotropy = 16\ndxvk.enableMemoryDefragX = False\nnoequals\n");
	Check(!v.memoryDefragOff && !v.cachedWriteOnlyOn, "other keys, a longer key, a line without '='");
	v = parse("dxvk.enableMemoryDefrag = False");
	Check(v.memoryDefragOff, "the last line without a newline");
	v = ParseDxvkConf(nullptr, 5);
	Check(!v.memoryDefragOff && !v.cachedWriteOnlyOn, "no text");
	// The Linux install's dxvk.conf of 2026-11-10, its comments and a third key.
	v = parse("# OBVR 2026-10-06: DXVK's memory defragmenter ... = here\n"
	          "dxvk.enableMemoryDefrag = False\n\n"
	          "d3d9.samplerAnisotropy = 16\n# d3d9.samplerLodBias = -0.3\n"
	          "d3d9.cachedWriteOnlyBuffers = True\n");
	Check(v.memoryDefragOff && v.cachedWriteOnlyOn, "the live file's shape");
}

void TestTrifleReader() {
	std::printf("Trifle.ini reader\n");
	auto parse = [](const char* text) { return ParseTrifleIni(text, static_cast<UInt32>(std::strlen(text))); };
	Check(parse("[General]\nEnableFirstPersonShadow=1\n") == Trifle::ShadowOn, "1: on");
	Check(parse("[General]\nEnableFirstPersonShadow = 0\n") == Trifle::ShadowOff, "0 with spaces: off");
	Check(parse("; EnableFirstPersonShadow=1\nEnableFirstPersonShadow=0\n") == Trifle::ShadowOff,
	      "a comment is skipped, the real line read");
	Check(parse("EnableFirstPersonShadowX=1\n") == Trifle::ShadowOff, "another key is not it");
	Check(parse("EnableFirstPersonShadow\n") == Trifle::ShadowOff, "no '=': not set");
	Check(parse("") == Trifle::ShadowOff, "an empty file: off");
	Check(ParseTrifleIni(nullptr, 0) == Trifle::NotInstalled, "no file: not installed");
}

void TestPeMachine() {
	std::printf("PE machine\n");
	UInt8 file[0x100] = {};
	file[0] = 'M';
	file[1] = 'Z';
	file[0x3C] = 0x80;
	file[0x80] = 'P';
	file[0x81] = 'E';
	file[0x84] = 0x4C;
	file[0x85] = 0x01;
	UInt16 machine = 0;
	Check(PeMachine(file, sizeof(file), machine) && machine == kMachineI386, "a 32-bit header");
	file[0x84] = 0x64;
	file[0x85] = 0x86;
	Check(PeMachine(file, sizeof(file), machine) && machine == 0x8664, "a 64-bit header");
	file[0] = 'X';
	Check(!PeMachine(file, sizeof(file), machine), "no MZ");
	file[0] = 'M';
	file[0x3C] = 0xFE;
	Check(!PeMachine(file, sizeof(file), machine), "the PE offset past the end");
	file[0x3C] = 0x80;
	file[0x81] = 'X';
	Check(!PeMachine(file, sizeof(file), machine), "no PE signature");
	Check(!PeMachine(file, 0x20, machine) && !PeMachine(nullptr, 0x100, machine), "too short, nothing");
}

void TestLogLines() {
	std::printf("Log lines\n");
	Facts f = Sound();
	f.dxvkConfFound = false;
	Result r[kCheckCount];
	Evaluate(f, r);
	char line[300];
	FormatLogLine(Of(r, C::DxvkConf), line, sizeof(line));
	Check(std::strncmp(line, "Self-check: dxvk.conf PROBLEM - ", 32) == 0, "a problem's line");
	FormatLogLine(Of(r, C::MenuQue), line, sizeof(line));
	Check(std::strcmp(line, "Self-check: menuque ok - MenuQue v16b") == 0, "an ok line");
	Result pending;
	pending.check = C::Dxvk;
	FormatLogLine(pending, line, sizeof(line));
	Check(std::strcmp(line, "Self-check: dxvk pending") == 0, "no text: no dash");
	Result unverified;
	unverified.check = C::Xobse;
	detail::Say(unverified, State::Unverified, "x");
	FormatLogLine(unverified, line, sizeof(line));
	Check(std::strcmp(line, "Self-check: xobse could not verify - x") == 0, "could not verify");
	FormatLogLine(pending, nullptr, 10);
	Check(true, "no buffer: nothing written, no crash");
	Check(!ShouldLog(State::Pending, State::Pending), "pending is never logged");
	Check(ShouldLog(State::Pending, State::Ok), "the first answer is logged");
	Check(!ShouldLog(State::Ok, State::Ok), "the same answer again is not");
	Check(ShouldLog(State::Ok, State::Hint), "a changed answer is");
	Check(std::strcmp(StateWord(State::Hint), "HINT") == 0 && std::strcmp(CheckName(C::SteamOverlay), "steam-overlay") == 0,
	      "the words");
}

void TestPanel() {
	std::printf("Panel\n");
	Facts f = Sound();
	f.device = Device::NotYet;
	Result r[kCheckCount];
	Evaluate(f, r);
	Check(!PanelReady(r), "a pending check holds the panel back");
	f = Sound();
	f.fullVr = true;  // the controllers still pending
	Evaluate(f, r);
	Check(PanelReady(r), "the controllers' pending does not");
	Check(!PanelWanted(r, true), "and nothing to say: no panel");
	f.dxvkConfFound = false;
	Evaluate(f, r);
	Check(PanelWanted(r, true), "a problem: the panel");
	Check(!PanelWanted(r, false), "a problem, the panel switched off: no panel");
	f = Sound();
	f.d3d9Gplasync = false;
	Evaluate(f, r);
	Check(PanelWanted(r, true), "a hint alone: the panel");
	f.overlayLoaded = true;
	Evaluate(f, r);
	char lines[kPanelLines][kPanelLineChars];
	UInt32 n = PanelLines(r, lines);
	Check(n == 2 && Has(lines[0], "Steam overlay") && std::strncmp(lines[1], "Hint: ", 6) == 0 && lines[2][0] == '\0',
	      "the problem first, the hint after it, the rest empty");
	// Six to say: four lines and a fifth that counts the rest.
	f = Sound();
	f.d3d9Gplasync = false;  // hint
	f.overlayLoaded = true;
	f.dxvkConfFound = false;
	f.menuQue = MenuQue::Missing;
	f.trifle = Trifle::ShadowOn;
	f.enhancedCamera = true;
	Evaluate(f, r);
	n = PanelLines(r, lines);
	Check(n == kPanelLines && Has(lines[4], "and 2 more") && !Has(lines[3], "Hint"),
	      "six: four problems shown, the hint among the two counted");
	// Exactly five: all fit, no counting line.
	f.enhancedCamera = false;
	Evaluate(f, r);
	n = PanelLines(r, lines);
	Check(n == 5 && Has(lines[4], "Hint: ") && !Has(lines[4], "more"), "five: all five, no count");
	Evaluate(Sound(), r);
	n = PanelLines(r, lines);
	Check(n == 0 && lines[0][0] == '\0', "nothing: no line");
}

void TestExpectedFiles() {
	std::printf("Expected files\n");
	bool unique = true;
	for (UInt32 i = 0; i < kExpectedFileCount; ++i) {
		for (UInt32 j = i + 1; j < kExpectedFileCount; ++j) {
			unique = unique && std::strcmp(kExpectedFiles[i].path, kExpectedFiles[j].path) != 0;
		}
	}
	Check(unique, "no file twice");
	bool selfCheck = false;
	for (const ExpectedFile& e : kExpectedFiles) {
		selfCheck = selfCheck || (!e.plugin && Has(e.path, "OBVR_SelfCheck.xml"));
	}
	Check(selfCheck, "the panel's own menu is among them");
}

}  // namespace

int main() {
	TestSound();
	TestOblivion();
	TestXobse();
	TestDxvk();
	TestDxvkConf();
	TestMenuQue();
	TestOverlay();
	TestFilesAndOpenVr();
	TestConflictsAndTrifle();
	TestControllers();
	TestObseLog();
	TestDxvkConfReader();
	TestTrifleReader();
	TestPeMachine();
	TestLogLines();
	TestPanel();
	TestExpectedFiles();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("All self-check checks passed\n");
	return 0;
}
