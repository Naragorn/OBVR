// Checks what a hand-script mark writes as pictures (test/MarkDump.h): the
// eyes, the 2D layer, both, neither, no layer to write, and the divisor -
// the default quarter, full size, held at 4.

#include <cstdio>
#include <cstring>

#include "test/MarkDump.h"

using namespace obvr::test;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

void Plans() {
	std::printf("plans:\n");
	MarkDumpPlan p = PlanMarkDump(false, true, true, true, 0);
	Check(!p.eyes && !p.layer, "no mark, nothing written");
	p = PlanMarkDump(true, true, false, true, 0);
	Check(p.eyes && !p.layer, "the eyes alone, as before");
	p = PlanMarkDump(true, false, true, true, 0);
	Check(!p.eyes && p.layer, "the layer alone");
	p = PlanMarkDump(true, true, true, true, 0);
	Check(p.eyes && p.layer, "both");
	p = PlanMarkDump(true, false, false, true, 0);
	Check(!p.eyes && !p.layer, "neither key, nothing");
	p = PlanMarkDump(true, true, true, false, 0);
	Check(p.eyes && !p.layer, "no layer captured this frame, the eyes still");
}

void Divisors() {
	std::printf("divisors:\n");
	Check(MarkDumpDivisor(0) == 4, "left out: a quarter, as before");
	Check(MarkDumpDivisor(1) == 1, "full size");
	Check(MarkDumpDivisor(2) == 2, "half size");
	Check(MarkDumpDivisor(4) == 4, "a quarter asked");
	Check(MarkDumpDivisor(8) == 4, "past 4 held at 4, the writer's smallest");
	Check(PlanMarkDump(true, true, false, true, 1).divisor == 1, "the plan carries the divisor");
	Check(PlanMarkDump(false, true, false, true, 0).divisor == 4, "unmarked plans the default too");
}

void Words() {
	std::printf("words:\n");
	MarkDumpPlan eyes = PlanMarkDump(true, true, false, true, 0);
	MarkDumpPlan layer = PlanMarkDump(true, false, true, true, 0);
	MarkDumpPlan both = PlanMarkDump(true, true, true, true, 0);
	Check(std::strcmp(MarkDumpWords(eyes, true, false), "written") == 0, "eyes alone written, the old line");
	Check(std::strcmp(MarkDumpWords(eyes, false, false), "refused") == 0, "eyes alone refused");
	Check(std::strcmp(MarkDumpWords(layer, false, true), "not asked, the layer written") == 0, "layer alone written");
	Check(std::strcmp(MarkDumpWords(layer, false, false), "not asked, the layer refused") == 0, "layer alone refused");
	Check(std::strncmp(MarkDumpWords(both, true, true), "written", 7) == 0, "both written still starts with written");
	Check(std::strcmp(MarkDumpWords(both, true, false), "written, the layer refused") == 0, "eyes yes, layer no");
	Check(std::strcmp(MarkDumpWords(both, false, true), "refused, the layer written") == 0, "eyes no, layer yes");
	Check(std::strcmp(MarkDumpWords(both, false, false), "refused, the layer refused") == 0, "both refused");
}

}  // namespace

int main() {
	Plans();
	Divisors();
	Words();
	std::printf(g_failures == 0 ? "all ok\n" : "%d FAILED\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
