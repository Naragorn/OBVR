// Checks the update notice's decisions: reading a release tag out of GitHub's
// answer, comparing versions, the line it shows, and how long it stays.

#include <cstdio>
#include <cstring>

#include "core/UpdateNotice.h"

using namespace obvr::update;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Tag(const char* json, char* out, UInt32 capacity = 32) {
	return ExtractTagName(json, static_cast<UInt32>(std::strlen(json)), out, capacity);
}

void TestParseVersion() {
	std::printf("Versions\n");
	UInt32 p[4];
	Check(ParseVersion("v0.2.3", p) && p[0] == 0 && p[1] == 2 && p[2] == 3 && p[3] == 0,
	      "a v-tag reads as its numbers");
	Check(ParseVersion("1.10", p) && p[0] == 1 && p[1] == 10 && p[2] == 0, "a short version pads with zeros");
	Check(ParseVersion("V1.2.3.4", p) && p[3] == 4, "four numbers and a capital V");
	Check(!ParseVersion("", p) && !ParseVersion("v", p) && !ParseVersion(nullptr, p), "empty refused");
	Check(!ParseVersion("1.2.3.4.5", p), "a fifth number refused");
	Check(!ParseVersion("1..2", p) && !ParseVersion(".1", p) && !ParseVersion("1.", p),
	      "an empty number refused");
	Check(!ParseVersion("0.2.3-beta", p) && !ParseVersion("0.2 ", p), "a suffix refused");
	Check(!ParseVersion("99999999999", p), "a number that does not fit refused");
}

void TestNewer() {
	std::printf("Comparison\n");
	Check(IsNewerVersion("v0.2.3", "0.2.2"), "patch newer");
	Check(IsNewerVersion("v0.3", "0.2.9"), "minor newer beats a larger patch");
	Check(IsNewerVersion("v1.0.0", "0.9.9"), "major newer");
	Check(IsNewerVersion("v0.2.10", "0.2.9"), "numbers compare as numbers, not text");
	Check(IsNewerVersion("v0.2.2.1", "0.2.2"), "a fourth number counts");
	Check(!IsNewerVersion("v0.2.2", "0.2.2"), "the same is not newer");
	Check(!IsNewerVersion("v0.2.1", "0.2.2"), "older is not newer");
	Check(!IsNewerVersion("garbage", "0.2.2") && !IsNewerVersion("v9", "dev"),
	      "anything unreadable is never newer");
}

void TestTag() {
	std::printf("Release answer\n");
	char out[32];
	Check(Tag("{\"url\":\"x\",\"tag_name\":\"v0.2.3\",\"name\":\"OBVR\"}", out) &&
	          std::strcmp(out, "v0.2.3") == 0,
	      "the tag is read");
	Check(Tag("{\"tag_name\" :\r\n \"v1.0\"}", out) && std::strcmp(out, "v1.0") == 0,
	      "whitespace around the colon");
	Check(!Tag("{\"name\":\"v0.2.3\"}", out) && out[0] == '\0', "no tag_name");
	Check(!Tag("{\"tag_name\":null}", out) && out[0] == '\0', "not a string");
	Check(!Tag("{\"tag_name\" \"v1\"}", out), "no colon");
	Check(!Tag("{\"tag_name\":\"v0.\\\"2\"}", out) && out[0] == '\0', "an escape refused");
	Check(!Tag("{\"tag_name\":\"v0.2.3", out) && out[0] == '\0', "cut off before the closing quote");
	Check(!Tag("{\"tag_name\":\"\"}", out), "empty tag");
	Check(!Tag("{\"tag_name\":\"v0.2.3\"}", out, 4) && out[0] == '\0', "does not fit");
	Check(!Tag("{\"tag_name\":\"v0.2.3\"}", nullptr, 4) && !Tag("{\"tag_name\":\"v\"}", out, 0),
	      "no room to write");
	Check(!ExtractTagName(nullptr, 5, out, sizeof(out)) && out[0] == '\0', "no answer");
	Check(!ExtractTagName("{\"tag_name\":\"v0.2.3\"}", 12, out, sizeof(out)),
	      "only the given length is read");
	Check(!Tag("{\"tag_nam", out), "the key cut off");
}

void TestFormat() {
	std::printf("Line\n");
	char out[64];
	Check(FormatNotice("v0.2.3", out, sizeof(out)) &&
	          std::strcmp(out, "An OBVR update is available! 0.2.3") == 0,
	      "the v is dropped");
	Check(FormatNotice("0.2.3", out, sizeof(out)) &&
	          std::strcmp(out, "An OBVR update is available! 0.2.3") == 0,
	      "a plain version as it is");
	Check(!FormatNotice("v0.2.3", out, 10) && out[0] == '\0', "does not fit");
	Check(!FormatNotice(nullptr, out, sizeof(out)) && out[0] == '\0', "no tag");
	Check(!FormatNotice("v1", nullptr, 8) && !FormatNotice("v1", out, 0), "no room");
}

void TestLifetime() {
	std::printf("Lifetime\n");
	{
		NoticeState s;
		Check(!StepNotice(s, false, false, 0.1f), "nothing to say in the main menu");
		Check(!StepNotice(s, false, true, 0.1f), "nothing to say in the world");
	}
	{
		NoticeState s;
		bool shown = true;
		for (int i = 0; i < 1000; ++i) {
			shown = StepNotice(s, true, false, 1.0f) && shown;
		}
		Check(shown, "the main menu keeps it however long one stays");
		Check(StepNotice(s, true, true, 29.0f), "still up 29 s into the world");
		Check(StepNotice(s, true, false, 0.0f), "a loading screen after that keeps it");
		Check(!StepNotice(s, true, true, 1.0f), "gone at 30 s in the world");
		Check(!StepNotice(s, true, false, 1.0f), "back in the main menu it stays gone");
	}
	{
		NoticeState s;
		StepNotice(s, false, true, 40.0f);
		Check(!StepNotice(s, true, false, 0.1f), "an answer that arrives after 30 s in the world is not shown");
	}
	{
		NoticeState s;
		StepNotice(s, true, true, 10.0f);
		Check(StepNotice(s, true, false, 100.0f), "time outside the world does not count");
		Check(StepNotice(s, true, true, -5.0f) && s.worldSeconds == 10.0f, "a negative step is ignored");
	}
}

}  // namespace

int main() {
	TestParseVersion();
	TestNewer();
	TestTag();
	TestFormat();
	TestLifetime();
	std::printf("Update notice: %d failures\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
