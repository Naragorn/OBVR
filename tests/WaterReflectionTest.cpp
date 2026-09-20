#include <cstdio>

#include "render/WaterReflection.h"

using obvr::render::BuildStableWaterReflectionConstant;
using obvr::render::IsWaterReflectionTargetCandidate;
using obvr::render::WaterReflectionConstant;
using obvr::render::WaterResourceAction;
using obvr::render::ChooseWaterResourceAction;

namespace {

unsigned g_failures = 0;
unsigned g_checks = 0;

void Check(bool condition, const char* message) {
	++g_checks;
	if (!condition) {
		++g_failures;
		std::printf("FAIL: %s\n", message);
	}
}

}  // namespace

int main() {
	for (unsigned enabled = 0; enabled < 2; ++enabled)
	for (unsigned currentKind = 0; currentKind < 3; ++currentKind)
	for (unsigned keptKind = 0; keptKind < 2; ++keptKind) {
		const UInt32 current = currentKind == 0 ? 0u :
		                       currentKind == 1 ? 0x1111u : 0x2222u;
		const UInt32 kept = keptKind == 0 ? 0u : 0x1111u;
		WaterResourceAction expected = WaterResourceAction::None;
		if (current != 0 && current != kept)
			expected = WaterResourceAction::Remember;
		else if (enabled && current == 0 && kept != 0)
			expected = WaterResourceAction::Restore;
		Check(ChooseWaterResourceAction(enabled != 0, current, kept) == expected,
		      "all water-resource lifecycle combinations");
	}

	for (unsigned index = 0; index < 2; ++index)
	for (unsigned present = 0; present < 2; ++present)
	for (unsigned backBuffer = 0; backBuffer < 2; ++backBuffer)
	for (unsigned substitute = 0; substitute < 2; ++substitute)
	for (unsigned readable = 0; readable < 2; ++readable)
	for (unsigned nonzero = 0; nonzero < 2; ++nonzero) {
		const bool expected = index == 0 && present && !backBuffer && !substitute &&
		                      readable && nonzero;
		Check(IsWaterReflectionTargetCandidate(index, present != 0, backBuffer != 0,
		                                         substitute != 0, readable != 0,
		                                         nonzero ? 512u : 0u,
		                                         nonzero ? 512u : 0u) == expected,
		      "all reflection-target classification gates");
	}
	Check(!IsWaterReflectionTargetCandidate(0, true, false, false, true, 512, 0),
	      "zero target height refused independently");

	float constants[48];
	for (unsigned i = 0; i < 48; ++i) constants[i] = static_cast<float>(i + 1);

	for (unsigned enabled = 0; enabled < 2; ++enabled)
	for (unsigned water = 0; water < 2; ++water)
	for (unsigned haveData = 0; haveData < 2; ++haveData) {
		WaterReflectionConstant out;
		out.value[0] = -1.0f;
		const bool accepted = BuildStableWaterReflectionConstant(
			enabled != 0, water != 0, 8, haveData ? constants : nullptr, 1, out);
		Check(accepted == (enabled && water && haveData),
		      "all enabled/water/data gate combinations");
		if (!accepted) Check(out.value[0] == -1.0f, "refusal preserves output");
	}

	struct Range { unsigned start; unsigned count; bool accepted; };
	const Range ranges[] = {
		{0,0,false}, {0,8,false}, {0,9,true}, {7,1,false},
		{7,2,true}, {8,1,true}, {9,1,false}, {0xFFFFFFFFu,2,false},
	};
	for (const Range& range : ranges) {
		WaterReflectionConstant out;
		Check(BuildStableWaterReflectionConstant(true, true, range.start, constants,
		                                         range.count, out) == range.accepted,
		      "all upload ranges around c8 are classified without overflow");
	}

	WaterReflectionConstant out;
	Check(BuildStableWaterReflectionConstant(true, true, 7, constants, 3, out),
	      "multi-register upload containing c8 accepted");
	Check(out.registerIndex == 8 && out.value[0] == constants[4] &&
	      out.value[1] == 0.0f && out.value[2] == constants[6] &&
	      out.value[3] == constants[7],
	      "only c8.y reflection-map amount is replaced");

	std::printf("%u checks, %u failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
