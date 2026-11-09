#include <cstdio>
#include <cstring>

#include "game/MessageBoxLogic.h"

using namespace obvr;
using namespace obvr::game;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

void TestCollect() {
	std::printf("The box's button labels\n");
	const char* out[kMessageBoxButtonsMost] = {};
	const char* rest[] = {"No", nullptr};
	Check(CollectMessageBoxButtons("Yes", rest, out, kMessageBoxButtonsMost) == 2 && std::strcmp(out[0], "Yes") == 0 &&
	          std::strcmp(out[1], "No") == 0,
	      "the first label, then the rest until their null");
	Check(CollectMessageBoxButtons("Ok", nullptr, out, kMessageBoxButtonsMost) == 1, "no rest: the first alone");
	const char* none[] = {nullptr};
	Check(CollectMessageBoxButtons("Ok", none, out, kMessageBoxButtonsMost) == 1, "a rest that ends at once");
	Check(CollectMessageBoxButtons(nullptr, rest, out, kMessageBoxButtonsMost) == 0, "no first label: no buttons");
	const char* emptyEnd[] = {"No", "", "Never"};
	Check(CollectMessageBoxButtons("Yes", emptyEnd, out, kMessageBoxButtonsMost) == 2,
	      "an empty label ends them, as in the builder");
	const char* many[] = {"b", "c", "d", nullptr};
	Check(CollectMessageBoxButtons("a", many, out, 2) == 2, "the capacity holds it");
	Check(CollectMessageBoxButtons("a", many, nullptr, 2) == 0, "nowhere to put them: none");
	Check(CollectMessageBoxButtons("a", many, out, 0) == 0, "no room: none");
}

void TestLine() {
	std::printf("The line\n");
	char line[96];
	const char* yesNo[] = {"Yes", "No"};
	ComposeMessageBoxLine(line, sizeof(line), "Buy 1 Iron Dagger for 10 gold?", yesNo, 2);
	Check(std::strcmp(line, "\"Buy 1 Iron Dagger for 10 gold?\" [Yes | No]") == 0, "the text and both buttons");
	ComposeMessageBoxLine(line, sizeof(line), "You do not have enough gold.", nullptr, 0);
	Check(std::strcmp(line, "\"You do not have enough gold.\" [no buttons]") == 0, "a box without buttons");
	const char* blank[] = {nullptr};
	ComposeMessageBoxLine(line, sizeof(line), "", blank, 1);
	Check(std::strcmp(line, "\"(empty text)\" []") == 0, "an empty text and a missing label");
	ComposeMessageBoxLine(line, sizeof(line), nullptr, nullptr, 0);
	Check(std::strcmp(line, "\"(no text)\" [no buttons]") == 0, "no text at all");
	char small[12];
	const UInt32 written = ComposeMessageBoxLine(small, sizeof(small), "A very long question indeed", yesNo, 2);
	Check(written == sizeof(small) - 1 && std::strlen(small) == sizeof(small) - 1, "cut to the buffer, ended");
	char tiny[3];
	ComposeMessageBoxLine(tiny, sizeof(tiny), "Text", yesNo, 2);
	Check(std::strlen(tiny) == 2, "a buffer too small for the first label: cut, ended");
	Check(ComposeMessageBoxLine(nullptr, 8, "x", yesNo, 2) == 0, "no buffer: nothing");
	Check(ComposeMessageBoxLine(line, 0, "x", yesNo, 2) == 0, "no room: nothing");
}

}  // namespace

int main() {
	TestCollect();
	TestLine();
	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
