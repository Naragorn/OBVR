#pragma once

#include "core/Types.h"

namespace obvr::render {

// The distant trees faced by their place, not by the view (Nadi in the
// headset, 2026-10-22: "die ganz weit entfernten bäume bewegen sich mit dem
// headset. das war mal auch so mit laub nah"). The distant land's trees are
// drawn instanced by the vertex shader DISTLOD2003 (shaderpackage*.sdp, read
// 2026-10-22): each instance's quad is turned about the vertical to stand
// across EyeDir (c14), the camera's view direction - so a turn of the head
// turns every distant tree with it, as the near trees' leaf cards did before
// render/LeafFacing.h. The shader already knows the eye's place (EyePos, c9)
// and the instance's (InstanceData, c20[a0]); this patch has it turn each
// quad across the direction from the eye to that tree instead, the leaf
// fix's rule. Pure: it reads the game's bytecode and writes a new one, the
// device makes it a shader (render/InterfaceRenderHook.cpp). Covered by
// distant_tree_facing_test.
//
// What is changed, nothing else: a prologue after the declarations computes
// the instance's index and eye - tree into a free temporary F, with copies of
// the shader's own instructions that do so later for the fog (frc, add, mova,
// mov from c20[a0], add with c9); every read of EyeDir is then a read of -F
// (EyeDir points from the eye; eye - tree points back). The quad's basis is
// built from the horizontal part only, as before.

constexpr UInt32 kShaderVersionMask = 0xFFFF0000u;
constexpr UInt32 kVertexShaderVersion = 0xFFFE0000u;
constexpr UInt32 kShaderEnd = 0x0000FFFFu;
constexpr UInt32 kShaderComment = 0xFFFEu;
constexpr UInt32 kCtabFourCC = 0x42415443u;  // "CTAB"
constexpr UInt32 kMaxPatchedTreeShaderDwords = 1024;

enum : UInt32 {
	kOpMov = 1,
	kOpAdd = 2,
	kOpFrc = 19,
	kOpDcl = 31,
	kOpMova = 46,
	kOpDef = 81,
};

enum : UInt32 { kRegTemp = 0, kRegInput = 1, kRegConst = 2, kRegAddress = 3 };

inline UInt32 ShaderRegType(UInt32 token) { return ((token >> 28) & 7u) | (((token >> 11) & 3u) << 3); }
inline UInt32 ShaderRegNumber(UInt32 token) { return token & 0x7FFu; }
inline bool ShaderRegRelative(UInt32 token) { return (token & 0x2000u) != 0; }
inline UInt32 ShaderSourceModifier(UInt32 token) { return (token >> 24) & 0xFu; }
inline UInt32 ShaderWithRegister(UInt32 token, UInt32 type, UInt32 number) {
	return (token & ~(0x70000000u | 0x1800u | 0x7FFu)) | ((type & 7u) << 28) | (((type >> 3) & 3u) << 11) |
	       (number & 0x7FFu);
}

// The constant table's register for a name, -1 without it.
inline int ShaderConstantRegister(const UInt32* code, UInt32 count, const char* name) {
	if (code == nullptr || count < 2) {
		return -1;
	}
	UInt32 at = 1;
	while (at < count) {
		const UInt32 token = code[at];
		if ((token & 0xFFFFu) != kShaderComment) {
			return -1;  // the table comes before the first instruction
		}
		const UInt32 length = (token >> 16) & 0x7FFFu;
		if (at + 1 + length > count) {
			return -1;
		}
		if (length >= 8 && code[at + 1] == kCtabFourCC) {
			const UInt8* table = reinterpret_cast<const UInt8*>(code + at + 2);
			const UInt32 tableBytes = (length - 1) * 4;
			const auto dword = [&](UInt32 offset) -> UInt32 {
				return offset + 4 <= tableBytes ? *reinterpret_cast<const UInt32*>(table + offset) : 0u;
			};
			const UInt32 constants = dword(12);
			const UInt32 info = dword(16);
			for (UInt32 i = 0; i < constants && info + i * 20 + 20 <= tableBytes; ++i) {
				const UInt32 entry = info + i * 20;
				const UInt32 nameAt = dword(entry);
				const UInt16 set = *reinterpret_cast<const UInt16*>(table + entry + 4);
				const UInt16 index = *reinterpret_cast<const UInt16*>(table + entry + 6);
				UInt32 k = 0;
				while (nameAt + k < tableBytes && name[k] != '\0' && table[nameAt + k] == static_cast<UInt8>(name[k])) {
					++k;
				}
				if (name[k] == '\0' && nameAt + k < tableBytes && table[nameAt + k] == 0 && set == 2) {
					return index;  // D3DXRS_FLOAT4
				}
			}
			return -1;
		}
		at += 1 + length;
	}
	return -1;
}

struct ShaderInstruction {
	UInt32 at = 0;      // its first token
	UInt32 length = 0;  // tokens after the instruction token
	UInt32 opcode = 0;
};

// The instructions of a shader model 2+ shader, comments skipped; false when
// it is not one or does not end.
inline bool ReadShaderInstructions(const UInt32* code, UInt32 count, ShaderInstruction* out, UInt32 capacity,
                                   UInt32& found) {
	found = 0;
	if (code == nullptr || count < 2 || (code[0] & kShaderVersionMask) != kVertexShaderVersion ||
	    ((code[0] >> 8) & 0xFFu) < 2) {
		return false;
	}
	UInt32 at = 1;
	while (at < count) {
		const UInt32 token = code[at];
		if (token == kShaderEnd) {
			return true;
		}
		if ((token & 0xFFFFu) == kShaderComment) {
			at += 1 + ((token >> 16) & 0x7FFFu);
			continue;
		}
		const UInt32 length = (token >> 24) & 0xFu;
		if (at + 1 + length > count || found >= capacity) {
			return false;
		}
		out[found++] = ShaderInstruction{at, length, token & 0xFFFFu};
		at += 1 + length;
	}
	return false;
}

// The index of an instruction's tokens that are registers (destination, then
// sources), the relative-address tokens left out.
inline UInt32 ShaderRegisterTokens(const UInt32* code, const ShaderInstruction& in, UInt32* at, UInt32 capacity) {
	UInt32 n = 0;
	if (in.opcode == kOpDcl || in.opcode == kOpDef) {
		return 0;
	}
	for (UInt32 i = 1; i <= in.length && n < capacity; ++i) {
		const UInt32 token = code[in.at + i];
		at[n++] = in.at + i;
		if (i > 1 && ShaderRegRelative(token)) {
			++i;  // its address token
		}
	}
	return n;
}

struct DistantTreeFacingResult {
	bool patched = false;
	const char* refusal = nullptr;
	UInt32 eyeDirReads = 0;
	UInt32 freeTemp = 0;
};

inline DistantTreeFacingResult PatchDistantTreeFacing(const UInt32* code, UInt32 count, UInt32* out,
                                                      UInt32 capacity, UInt32& outCount) {
	DistantTreeFacingResult r;
	outCount = 0;
	const int eyeDir = ShaderConstantRegister(code, count, "EyeDir");
	const int eyePos = ShaderConstantRegister(code, count, "EyePos");
	const int instances = ShaderConstantRegister(code, count, "InstanceData");
	if (eyeDir < 0 || eyePos < 0 || instances < 0) {
		r.refusal = "not the distant trees' shader (EyeDir, EyePos and InstanceData not all in its table)";
		return r;
	}
	ShaderInstruction ins[256];
	UInt32 n = 0;
	if (!ReadShaderInstructions(code, count, ins, 256, n)) {
		r.refusal = "not a shader model 2 vertex shader that could be read to its end";
		return r;
	}
	// The temporaries in use, and the five instructions to copy.
	UInt32 maxTemp = 0;
	bool anyTemp = false;
	int frc = -1, floorAdd = -1, mova = -1, fetch = -1, toEye = -1;
	UInt32 fracTemp = 0, floorTemp = 0, placeTemp = 0;
	for (UInt32 i = 0; i < n; ++i) {
		UInt32 regs[8];
		const UInt32 k = ShaderRegisterTokens(code, ins[i], regs, 8);
		for (UInt32 j = 0; j < k; ++j) {
			if (ShaderRegType(code[regs[j]]) == kRegTemp) {
				anyTemp = true;
				maxTemp = ShaderRegNumber(code[regs[j]]) > maxTemp ? ShaderRegNumber(code[regs[j]]) : maxTemp;
			}
		}
		if (ins[i].opcode == kOpFrc && frc < 0 && k == 2 && ShaderRegType(code[regs[0]]) == kRegTemp &&
		    ShaderRegType(code[regs[1]]) == kRegInput) {
			frc = static_cast<int>(i);
			fracTemp = ShaderRegNumber(code[regs[0]]);
		} else if (ins[i].opcode == kOpAdd && frc >= 0 && floorAdd < 0 && k == 3 &&
		           ShaderRegType(code[regs[0]]) == kRegTemp &&
		           ShaderRegType(code[regs[1]]) == kRegTemp && ShaderRegNumber(code[regs[1]]) == fracTemp &&
		           ShaderRegType(code[regs[2]]) == kRegInput) {
			floorAdd = static_cast<int>(i);
			floorTemp = ShaderRegNumber(code[regs[0]]);
		} else if (ins[i].opcode == kOpMova && floorAdd >= 0 && mova < 0 && k == 2 &&
		           ShaderRegType(code[regs[1]]) == kRegTemp && ShaderRegNumber(code[regs[1]]) == floorTemp) {
			mova = static_cast<int>(i);
		} else if (ins[i].opcode == kOpMov && mova >= 0 && fetch < 0 && k == 2 &&
		           ShaderRegType(code[regs[0]]) == kRegTemp && ShaderRegType(code[regs[1]]) == kRegConst &&
		           ShaderRegRelative(code[regs[1]]) &&
		           ShaderRegNumber(code[regs[1]]) == static_cast<UInt32>(instances)) {
			fetch = static_cast<int>(i);
			placeTemp = ShaderRegNumber(code[regs[0]]);
		} else if (ins[i].opcode == kOpAdd && fetch >= 0 && toEye < 0 && k == 3 &&
		           ShaderRegType(code[regs[0]]) == kRegTemp && ShaderRegNumber(code[regs[0]]) == placeTemp) {
			const bool a = ShaderRegType(code[regs[1]]) == kRegTemp && ShaderRegNumber(code[regs[1]]) == placeTemp &&
			               ShaderSourceModifier(code[regs[1]]) == 1 && ShaderRegType(code[regs[2]]) == kRegConst &&
			               ShaderRegNumber(code[regs[2]]) == static_cast<UInt32>(eyePos);
			if (a) {
				toEye = static_cast<int>(i);
			}
		}
	}
	if (frc < 0 || floorAdd < 0 || mova < 0 || fetch < 0 || toEye < 0) {
		r.refusal = "the instance's index and its way to the eye were not found as expected";
		return r;
	}
	r.freeTemp = anyTemp ? maxTemp + 1 : 0;
	if (r.freeTemp >= 12) {
		r.refusal = "no temporary register left";
		return r;
	}
	// The first instruction that is not a declaration: the prologue goes there.
	UInt32 first = 0;
	while (first < n && (ins[first].opcode == kOpDcl || ins[first].opcode == kOpDef)) {
		++first;
	}
	const int copies[5] = {frc, floorAdd, mova, fetch, toEye};
	UInt32 w = 0;
	const auto put = [&](UInt32 token) {
		if (w < capacity) {
			out[w] = token;
		}
		++w;
	};
	const auto copyPrologue = [&]() {
		for (const int c : copies) {
			const ShaderInstruction& in = ins[c];
			UInt32 regs[8];
			const UInt32 k = ShaderRegisterTokens(code, in, regs, 8);
			for (UInt32 t = 0; t <= in.length; ++t) {
				UInt32 token = code[in.at + t];
				for (UInt32 j = 0; j < k; ++j) {
					if (regs[j] == in.at + t && ShaderRegType(token) == kRegTemp) {
						token = ShaderWithRegister(token, kRegTemp, r.freeTemp);
					}
				}
				put(token);
			}
		}
	};
	// Everything up to the first instruction as it was (the version, the
	// comments, the declarations and definitions).
	const UInt32 head = first < n ? ins[first].at : count - 1;
	for (UInt32 t = 0; t < head; ++t) {
		put(code[t]);
	}
	copyPrologue();
	for (UInt32 i = first; i < n; ++i) {
		const ShaderInstruction& in = ins[i];
		UInt32 regs[8];
		const UInt32 k = ShaderRegisterTokens(code, in, regs, 8);
		for (UInt32 t = 0; t <= in.length; ++t) {
			UInt32 token = code[in.at + t];
			for (UInt32 j = 1; j < k; ++j) {  // sources only
				if (regs[j] == in.at + t && ShaderRegType(token) == kRegConst && !ShaderRegRelative(token) &&
				    ShaderRegNumber(token) == static_cast<UInt32>(eyeDir)) {
					const UInt32 modifier = ShaderSourceModifier(token);
					if (modifier > 1) {
						r.refusal = "EyeDir is read with a modifier other than a negation";
						outCount = 0;
						return r;
					}
					token = ShaderWithRegister(token, kRegTemp, r.freeTemp);
					token = (token & ~0x0F000000u) | ((modifier == 1 ? 0u : 1u) << 24);
					++r.eyeDirReads;
				}
			}
			put(token);
		}
	}
	put(kShaderEnd);
	if (r.eyeDirReads == 0) {
		r.refusal = "EyeDir is never read";
		return r;
	}
	if (w > capacity) {
		r.refusal = "the patched shader is larger than the buffer";
		return r;
	}
	outCount = w;
	r.patched = true;
	return r;
}

}  // namespace obvr::render
