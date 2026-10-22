#include "render/DistantTreeShader.h"

#include "core/Log.h"
#include "render/D3D9Types.h"
#include "render/DistantTreeFacing.h"

namespace obvr::render {
namespace {

using GetFunctionFn = SInt32(__stdcall*)(void* self, void* data, UInt32* size);
constexpr UInt32 kShaderGetFunction = 4;
constexpr UInt32 kMaxShaderDwords = 1024;

// Each shader the game has set, once: its replacement, or null for one left
// as it is. Open addressing on the pointer; the game makes a few hundred.
constexpr UInt32 kSlots = 2048;
struct Slot {
	void* shader;
	void* replacement;
};
Slot g_slots[kSlots] = {};
UInt32 g_used = 0;
void* g_device = nullptr;
bool g_fullSaid = false;
bool g_drawnSaid = false;

UInt32 SlotOf(void* shader) {
	UInt32 h = reinterpret_cast<UInt32>(shader) >> 4;
	h ^= h >> 11;
	return (h * 2654435761u) & (kSlots - 1);
}

void* Examine(void* device, void* shader) {
	auto getFunction = d3d9::Method<GetFunctionFn>(shader, kShaderGetFunction);
	UInt32 bytes = 0;
	if (getFunction == nullptr || getFunction(shader, nullptr, &bytes) < 0 || bytes == 0 ||
	    bytes > kMaxShaderDwords * 4 || bytes % 4 != 0) {
		return nullptr;
	}
	static UInt32 code[kMaxShaderDwords];
	if (getFunction(shader, code, &bytes) < 0) {
		return nullptr;
	}
	if (ShaderConstantRegister(code, bytes / 4, "EyeDir") < 0) {
		return nullptr;  // the common case: any other shader
	}
	static UInt32 patched[kMaxPatchedTreeShaderDwords];
	UInt32 count = 0;
	const DistantTreeFacingResult r = PatchDistantTreeFacing(code, bytes / 4, patched, kMaxPatchedTreeShaderDwords, count);
	if (!r.patched) {
		OBVR_LOG("Distant trees: shader %p reads EyeDir but is left as it is - %s", shader, r.refusal);
		return nullptr;
	}
	auto create = d3d9::Method<d3d9::CreateVertexShaderFn>(device, d3d9::kDeviceCreateVertexShader);
	void* replacement = nullptr;
	const SInt32 hr = create != nullptr ? create(device, patched, &replacement) : -1;
	if (hr < 0 || replacement == nullptr) {
		OBVR_LOG("Distant trees: the patched shader for %p could not be made (0x%08X) - left as it is", shader,
		         static_cast<UInt32>(hr));
		return nullptr;
	}
	OBVR_LOG("Distant trees: shader %p turns its trees by EyeDir - replaced by %p, faced by each tree's place "
	         "(%u EyeDir reads, r%u free, %u dwords)",
	         shader, replacement, r.eyeDirReads, r.freeTemp, count);
	return replacement;
}

}  // namespace

void* SelectDistantTreeShader(void* device, void* shader, bool enabled) {
	if (shader == nullptr || device == nullptr) {
		return shader;
	}
	if (device != g_device) {
		// Another device: its shaders are other objects (the replacements of
		// the old one die with it).
		for (Slot& s : g_slots) {
			s = Slot{};
		}
		g_used = 0;
		g_device = device;
	}
	UInt32 at = SlotOf(shader);
	for (UInt32 probe = 0; probe < kSlots; ++probe, at = (at + 1) & (kSlots - 1)) {
		Slot& s = g_slots[at];
		if (s.shader == shader) {
			if (enabled && s.replacement != nullptr) {
				if (!g_drawnSaid) {
					g_drawnSaid = true;
					OBVR_LOG("Distant trees: the place-faced shader set for the first time");
				}
				return s.replacement;
			}
			return shader;
		}
		if (s.shader == nullptr) {
			if (g_used + 1 >= kSlots * 3 / 4) {
				if (!g_fullSaid) {
					g_fullSaid = true;
					OBVR_LOG("Distant trees: more shaders than the table holds - new ones are left as they are");
				}
				return shader;
			}
			s.shader = shader;
			s.replacement = Examine(device, shader);
			++g_used;
			return enabled && s.replacement != nullptr ? s.replacement : shader;
		}
	}
	return shader;
}

}  // namespace obvr::render
