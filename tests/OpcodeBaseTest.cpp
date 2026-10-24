// Checks the opcode base OBVR claims from xOBSE (obse/OpcodeBase.h): xOBSE
// keys a plugin's co-save chunk by its opcode base and skips the save
// callback of a plugin that never claimed one - the quick-key pages were
// never written until 2026-10-24.

#include <cstdio>

#include "obse/OpcodeBase.h"

using namespace obvr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

UInt32 g_claimed = 0;
int g_calls = 0;

void FakeSetOpcodeBase(UInt32 opcode) {
	g_claimed = opcode;
	++g_calls;
}

obse::Interface FakeInterface() {
	obse::Interface obse{};
	obse.SetOpcodeBase = FakeSetOpcodeBase;
	return obse;
}

void TestJudge() {
	std::printf("Which bases xOBSE takes as a plugin's own\n");
	Check(obse::JudgeOpcodeBase(obse::kOpcodeBase) == obse::OpcodeBaseVerdict::Own, "OBVR's base is its own");
	Check(obse::JudgeOpcodeBase(0x2000) == obse::OpcodeBaseVerdict::Default,
	      "0x2000 is xOBSE's default: the save callback is skipped");
	Check(obse::JudgeOpcodeBase(0x1FFF) == obse::OpcodeBaseVerdict::OutOfRange, "below 0x2000 xOBSE asserts");
	Check(obse::JudgeOpcodeBase(0x8000) == obse::OpcodeBaseVerdict::OutOfRange, "0x8000 and above xOBSE asserts");
	Check(obse::JudgeOpcodeBase(0x7FFF) == obse::OpcodeBaseVerdict::Own, "0x7FFF is the last base xOBSE takes");
	Check(obse::kOpcodeBase < 0x2B00 || obse::kOpcodeBase > 0x2B0F, "off OblivionWalkers' 0x2B00 to 0x2B0F");
	Check(obse::kOpcodeBase > 0x2A24, "past the ranges the xOBSE wiki lists (last 0x2A24)");
}

void TestClaim() {
	std::printf("The claim at load\n");
	g_claimed = 0;
	g_calls = 0;
	obse::Interface obse = FakeInterface();
	Check(obse::ClaimOpcodeBase(&obse, obse::kOpcodeBase) == obse::OpcodeBaseVerdict::Own, "claimed");
	Check(g_calls == 1 && g_claimed == obse::kOpcodeBase, "SetOpcodeBase called once with OBVR's base");

	g_calls = 0;
	Check(obse::ClaimOpcodeBase(nullptr, obse::kOpcodeBase) == obse::OpcodeBaseVerdict::NoCall, "no interface: no call");
	obse::Interface bare{};
	Check(obse::ClaimOpcodeBase(&bare, obse::kOpcodeBase) == obse::OpcodeBaseVerdict::NoCall,
	      "an interface without SetOpcodeBase: no call");
	Check(obse::ClaimOpcodeBase(&obse, 0x2000) == obse::OpcodeBaseVerdict::Default, "the default is not claimed");
	Check(obse::ClaimOpcodeBase(&obse, 0x9000) == obse::OpcodeBaseVerdict::OutOfRange, "out of range is not claimed");
	Check(g_calls == 0, "none of the refusals called xOBSE");
}

}  // namespace

int main() {
	TestJudge();
	TestClaim();
	if (g_failures != 0) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("all ok\n");
	return 0;
}
