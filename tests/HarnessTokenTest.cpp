// Checks the test overlay's gate (core/HarnessToken.h): OBVR-test.ini is
// read over OBVR.ini only when the token it names is the one the runner put
// into the game's environment - a game started through Steam carries none,
// a file left behind names another run's.

#include <cstdio>
#include <cstring>

#include "core/HarnessToken.h"

using namespace obvr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

void TestVerdicts() {
	std::printf("The overlay's token against the environment's\n");
	Check(JudgeHarnessOverlay("20261023-193000-4242", "20261023-193000-4242") == HarnessOverlayVerdict::Accepted,
	      "the same token on both sides: accepted");
	Check(JudgeHarnessOverlay("20261023-193000-4242", "20261023-193512-4300") == HarnessOverlayVerdict::Mismatch,
	      "another run's token: mismatch");
	Check(JudgeHarnessOverlay("20261023-193000-4242", "20261023-193000") == HarnessOverlayVerdict::Mismatch,
	      "a prefix is not the token");
	Check(JudgeHarnessOverlay("Run-1", "run-1") == HarnessOverlayVerdict::Mismatch, "case counts");
	Check(JudgeHarnessOverlay("", "20261023-193000-4242") == HarnessOverlayVerdict::NoFileToken,
	      "a file without a token: refused as such");
	Check(JudgeHarnessOverlay(nullptr, "20261023-193000-4242") == HarnessOverlayVerdict::NoFileToken,
	      "a null file token: the same");
	Check(JudgeHarnessOverlay("20261023-193000-4242", "") == HarnessOverlayVerdict::NoEnvToken,
	      "a game without the environment's token: refused as such");
	Check(JudgeHarnessOverlay("20261023-193000-4242", nullptr) == HarnessOverlayVerdict::NoEnvToken,
	      "a null environment token: the same");
	Check(JudgeHarnessOverlay("", "") == HarnessOverlayVerdict::NoFileToken,
	      "neither: the file's missing token is named first");
}

void TestText() {
	std::printf("The words for the log\n");
	const char* accepted = HarnessOverlayVerdictText(HarnessOverlayVerdict::Accepted);
	const char* noFile = HarnessOverlayVerdictText(HarnessOverlayVerdict::NoFileToken);
	const char* noEnv = HarnessOverlayVerdictText(HarnessOverlayVerdict::NoEnvToken);
	const char* mismatch = HarnessOverlayVerdictText(HarnessOverlayVerdict::Mismatch);
	Check(std::strstr(accepted, "read over OBVR.ini") != nullptr, "accepted: read over OBVR.ini, as the line always said");
	Check(std::strstr(noFile, "IGNORED") != nullptr && std::strstr(noEnv, "IGNORED") != nullptr &&
	          std::strstr(mismatch, "IGNORED") != nullptr,
	      "every refusal says IGNORED");
	Check(std::strcmp(noFile, noEnv) != 0 && std::strcmp(noEnv, mismatch) != 0 && std::strcmp(noFile, mismatch) != 0,
	      "and each refusal has its own words");
	Check(std::strstr(noEnv, "OBVR_HARNESS") != nullptr, "a game without the token is told the variable's name");
}

}  // namespace

int main() {
	TestVerdicts();
	TestText();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
