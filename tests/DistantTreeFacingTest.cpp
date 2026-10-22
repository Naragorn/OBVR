// Checks the distant trees' facing patch (render/DistantTreeFacing.h): on a
// shader shaped like the game's DISTLOD2003 - its constant table, its use of
// EyeDir and its own instance fetch - the prologue goes in after the
// declarations, every EyeDir read becomes the negated free temporary, nothing
// else changes; and every way it refuses.

#include <cstdio>
#include <cstring>
#include <vector>

#include "render/DistantTreeFacing.h"

namespace {

using namespace obvr::render;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

constexpr UInt32 kXYZW = 0xE4, kXXXX = 0x00, kWWWW = 0xFF, kXYXW = 0xC4, kXYYW = 0xD4;

UInt32 Ins(UInt32 op, UInt32 len) { return op | (len << 24); }
UInt32 Dst(UInt32 type, UInt32 n, UInt32 mask) {
	return 0x80000000u | ((type & 7u) << 28) | (((type >> 3) & 3u) << 11) | (mask << 16) | n;
}
UInt32 Src(UInt32 type, UInt32 n, UInt32 swizzle, bool negate = false, bool relative = false) {
	return 0x80000000u | ((type & 7u) << 28) | (((type >> 3) & 3u) << 11) | (swizzle << 16) |
	       ((negate ? 1u : 0u) << 24) | (relative ? 0x2000u : 0u) | n;
}
UInt32 Addr(UInt32 swizzle) { return 0x80000000u | (3u << 28) | (swizzle << 16); }

struct Constant {
	const char* name;
	UInt32 reg;
};

// A CTAB comment with the given float constants.
std::vector<UInt32> Ctab(const std::vector<Constant>& constants) {
	std::vector<unsigned char> t(28, 0);
	const auto put32 = [&](size_t at, UInt32 v) { std::memcpy(&t[at], &v, 4); };
	const UInt32 info = 28;
	t.resize(info + 20 * constants.size(), 0);
	put32(12, static_cast<UInt32>(constants.size()));
	put32(16, info);
	for (size_t i = 0; i < constants.size(); ++i) {
		const UInt32 nameAt = static_cast<UInt32>(t.size());
		for (const char* c = constants[i].name; *c != '\0'; ++c) {
			t.push_back(static_cast<unsigned char>(*c));
		}
		t.push_back(0);
		const size_t e = info + 20 * i;
		put32(e, nameAt);
		const unsigned short set = 2, reg = static_cast<unsigned short>(constants[i].reg), regCount = 1;
		std::memcpy(&t[e + 4], &set, 2);
		std::memcpy(&t[e + 6], &reg, 2);
		std::memcpy(&t[e + 8], &regCount, 2);
	}
	while (t.size() % 4 != 0) {
		t.push_back(0);
	}
	std::vector<UInt32> out;
	const UInt32 length = static_cast<UInt32>(t.size() / 4 + 1);
	out.push_back(0xFFFEu | (length << 16));
	out.push_back(kCtabFourCC);
	for (size_t i = 0; i < t.size(); i += 4) {
		UInt32 v;
		std::memcpy(&v, &t[i], 4);
		out.push_back(v);
	}
	return out;
}

// The game's shape: two EyeDir reads, then the index and the way to the eye.
std::vector<UInt32> TreeShader(const std::vector<Constant>& constants, bool withFetch = true,
                               bool eyeDirAbs = false) {
	std::vector<UInt32> s{0xFFFE0200u};
	const std::vector<UInt32> table = Ctab(constants);
	s.insert(s.end(), table.begin(), table.end());
	s.insert(s.end(), {Ins(kOpDcl, 2), 0x80000000u, Dst(kRegInput, 0, 0xF)});
	s.insert(s.end(), {Ins(kOpDcl, 2), 0x80010005u, Dst(kRegInput, 3, 0xF)});
	s.insert(s.end(), {Ins(kOpDef, 5), Dst(kRegConst, 6, 0xF), 0xBF800000u, 0, 0x3F800000u, 0x3C23D70Au});
	s.insert(s.end(), {Ins(kOpMov, 2), Dst(kRegTemp, 2, 0xB), Src(kRegConst, 6, kXYZW)});
	s.insert(s.end(), {Ins(5, 3), Dst(kRegTemp, 0, 0x7), Src(kRegTemp, 2, 0x40), Src(kRegConst, 14, kXYXW)});
	UInt32 second = Src(kRegConst, 14, kXYYW, true);
	if (eyeDirAbs) {
		second = (second & ~0x0F000000u) | (0xBu << 24);
	}
	s.insert(s.end(), {Ins(5, 3), Dst(kRegTemp, 1, 0x5), Src(kRegTemp, 0, kWWWW), second});
	s.insert(s.end(), {Ins(kOpFrc, 2), Dst(kRegTemp, 1, 0x8), Src(kRegInput, 3, kXXXX)});
	s.insert(s.end(), {Ins(kOpAdd, 3), Dst(kRegTemp, 0, 0x8), Src(kRegTemp, 1, kWWWW, true), Src(kRegInput, 3, kXXXX)});
	s.insert(s.end(), {Ins(kOpMova, 2), Dst(kRegAddress, 0, 0x8), Src(kRegTemp, 0, kWWWW)});
	if (withFetch) {
		s.insert(s.end(), {Ins(kOpMov, 3), Dst(kRegTemp, 1, 0x7), Src(kRegConst, 20, kXYZW, false, true), Addr(kWWWW)});
		s.insert(s.end(), {Ins(kOpAdd, 3), Dst(kRegTemp, 1, 0x7), Src(kRegTemp, 1, kXYZW, true), Src(kRegConst, 9, kXYZW)});
	}
	s.insert(s.end(), {Ins(kOpMov, 2), Dst(4u, 0, 0xF), Src(kRegTemp, 1, kXYZW)});  // oPos
	s.push_back(kShaderEnd);
	return s;
}

const std::vector<Constant> kTable{{"EyeDir", 14}, {"EyePos", 9}, {"InstanceData", 20}, {"ModelViewProj", 0}};

void TestTable() {
	std::printf("The constant table\n");
	const std::vector<UInt32> s = TreeShader(kTable);
	Check(ShaderConstantRegister(s.data(), static_cast<UInt32>(s.size()), "EyeDir") == 14 &&
	          ShaderConstantRegister(s.data(), static_cast<UInt32>(s.size()), "EyePos") == 9 &&
	          ShaderConstantRegister(s.data(), static_cast<UInt32>(s.size()), "InstanceData") == 20,
	      "the names give their registers");
	Check(ShaderConstantRegister(s.data(), static_cast<UInt32>(s.size()), "Eye") == -1 &&
	          ShaderConstantRegister(s.data(), static_cast<UInt32>(s.size()), "EyeDirX") == -1,
	      "a name is matched whole, not by its start");
	Check(ShaderConstantRegister(nullptr, 0, "EyeDir") == -1, "no shader: none");
}

void TestPatch() {
	std::printf("The patch on the game's shape\n");
	const std::vector<UInt32> s = TreeShader(kTable);
	UInt32 out[kMaxPatchedTreeShaderDwords];
	UInt32 count = 0;
	const DistantTreeFacingResult r =
		PatchDistantTreeFacing(s.data(), static_cast<UInt32>(s.size()), out, kMaxPatchedTreeShaderDwords, count);
	Check(r.patched && r.refusal == nullptr, "patched");
	Check(r.eyeDirReads == 2 && r.freeTemp == 3, "both EyeDir reads changed, r3 the free temporary");
	Check(count == s.size() + 3 + 4 + 3 + 4 + 4, "longer by exactly the five copied instructions");
	ShaderInstruction ins[64];
	UInt32 n = 0;
	Check(ReadShaderInstructions(out, count, ins, 64, n) && out[count - 1] == kShaderEnd, "it reads to its end");
	// The prologue after the declarations: frc, add, mova, mov, add - on r3.
	Check(n >= 8 && ins[3].opcode == kOpFrc && ins[4].opcode == kOpAdd && ins[5].opcode == kOpMova &&
	          ins[6].opcode == kOpMov && ins[7].opcode == kOpAdd,
	      "the prologue follows the declarations and definitions");
	Check(ShaderRegNumber(out[ins[3].at + 1]) == 3 && ShaderRegNumber(out[ins[4].at + 1]) == 3 &&
	          ShaderRegNumber(out[ins[4].at + 2]) == 3 && ShaderRegNumber(out[ins[5].at + 2]) == 3 &&
	          ShaderRegNumber(out[ins[6].at + 1]) == 3 && ShaderRegNumber(out[ins[7].at + 1]) == 3 &&
	          ShaderRegNumber(out[ins[7].at + 2]) == 3,
	      "its temporaries are all the free one");
	Check(ShaderRegType(out[ins[3].at + 2]) == kRegInput && ShaderRegNumber(out[ins[3].at + 2]) == 3 &&
	          ShaderRegRelative(out[ins[6].at + 2]) && ShaderRegNumber(out[ins[6].at + 2]) == 20 &&
	          ShaderRegType(out[ins[7].at + 3]) == kRegConst && ShaderRegNumber(out[ins[7].at + 3]) == 9,
	      "its inputs as the game's: v3, c20[a0] and EyePos");
	// The EyeDir reads: the free temporary, the negation turned over.
	const UInt32 first = out[ins[9].at + 3];
	const UInt32 second = out[ins[10].at + 3];
	Check(ShaderRegType(first) == kRegTemp && ShaderRegNumber(first) == 3 && ShaderSourceModifier(first) == 1 &&
	          ((first >> 16) & 0xFF) == kXYXW,
	      "EyeDir.xyxw read as -r3.xyxw");
	Check(ShaderRegType(second) == kRegTemp && ShaderRegNumber(second) == 3 && ShaderSourceModifier(second) == 0 &&
	          ((second >> 16) & 0xFF) == kXYYW,
	      "-EyeDir.xyyw read as r3.xyyw");
	// Everything after the prologue is the original, but for those two tokens.
	UInt32 differ = 0;
	const UInt32 shift = 3 + 4 + 3 + 4 + 4;
	const UInt32 firstAt = ins[3].at;
	for (UInt32 i = firstAt + shift; i < count; ++i) {
		differ += out[i] != s[i - shift] ? 1u : 0u;
	}
	UInt32 headDiffer = 0;
	for (UInt32 i = 0; i < firstAt; ++i) {
		headDiffer += out[i] != s[i] ? 1u : 0u;
	}
	Check(differ == 2 && headDiffer == 0, "nothing else changed: the head as it was, two tokens after it");
}

void TestRefusals() {
	std::printf("What it refuses\n");
	UInt32 out[kMaxPatchedTreeShaderDwords];
	UInt32 count = 0;
	{
		const std::vector<UInt32> s = TreeShader({{"EyePos", 9}, {"InstanceData", 20}});
		const DistantTreeFacingResult r =
			PatchDistantTreeFacing(s.data(), static_cast<UInt32>(s.size()), out, kMaxPatchedTreeShaderDwords, count);
		Check(!r.patched && r.refusal != nullptr && count == 0, "no EyeDir in the table: another shader, left alone");
	}
	{
		const std::vector<UInt32> s = TreeShader(kTable, false);
		const DistantTreeFacingResult r =
			PatchDistantTreeFacing(s.data(), static_cast<UInt32>(s.size()), out, kMaxPatchedTreeShaderDwords, count);
		Check(!r.patched && count == 0, "no instance fetch to copy: refused");
	}
	{
		const std::vector<UInt32> s = TreeShader(kTable, true, true);
		const DistantTreeFacingResult r =
			PatchDistantTreeFacing(s.data(), static_cast<UInt32>(s.size()), out, kMaxPatchedTreeShaderDwords, count);
		Check(!r.patched && count == 0, "EyeDir read with another modifier: refused");
	}
	{
		std::vector<UInt32> s = TreeShader(kTable);
		s[0] = 0xFFFE0101u;
		const DistantTreeFacingResult r =
			PatchDistantTreeFacing(s.data(), static_cast<UInt32>(s.size()), out, kMaxPatchedTreeShaderDwords, count);
		Check(!r.patched && count == 0, "shader model 1: refused");
	}
	{
		std::vector<UInt32> s = TreeShader(kTable);
		s.pop_back();
		const DistantTreeFacingResult r =
			PatchDistantTreeFacing(s.data(), static_cast<UInt32>(s.size()), out, kMaxPatchedTreeShaderDwords, count);
		Check(!r.patched && count == 0, "no end token: refused");
	}
	{
		const std::vector<UInt32> s = TreeShader(kTable);
		const DistantTreeFacingResult r =
			PatchDistantTreeFacing(s.data(), static_cast<UInt32>(s.size()), out, 10, count);
		Check(!r.patched && count == 0, "a buffer too small: refused");
	}
}

}  // namespace

int main() {
	TestTable();
	TestPatch();
	TestRefusals();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
