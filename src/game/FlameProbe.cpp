#include "game/FlameProbe.h"

#include "core/Log.h"
#include "core/AddressSpace.h"
#include "game/FirstPersonHide.h"
#include "game/FlameProbeLogic.h"
#include "game/GameAddresses.h"
#include "game/GameTypes.h"
#include "game/PlayerBody.h"

namespace obvr::game {
namespace {

// NiObjectNET's first controller (game/EnemyHealthModel.cpp, read off the
// enemy health tile's flip controller), NiAVObject's property list - a
// NiTListBase at +0x98 whose head is at +0x9C, a node {next, prev, data}.
constexpr UInt32 kControllerOffset = 0x0C;
constexpr UInt32 kPropertyListHeadOffset = 0x9C;
constexpr UInt32 kListNodeNextOffset = 0x00;
constexpr UInt32 kListNodeDataOffset = 0x08;
constexpr UInt32 kDepthMost = 10;
constexpr UInt32 kChainMost = 8;
constexpr UInt32 kPropertiesMost = 12;
constexpr UInt32 kLinesMost = 120;
constexpr UInt32 kCallsMost = 40;

UInt32 g_lines = 0;
UInt32 g_calls = 0;
// The first-person flame's particle emitter and update controllers, found by
// the last mark's walk, and how many frames their times are still logged
// around the world renders (ProbeFlameTimes).
const UInt8* g_watched[2] = {};
UInt32 g_watchFrames = 0;
// The billboard the flame's particles hang under ("FireParticles-Emitter"):
// NiBillboardNode keeps the time of its last downward update at +0xE0 and
// hands it, with the controllers on, to its children when it is culled
// (0x007219C0 stores it, 0x00721B70 replays it).
const UInt8* g_billboard = nullptr;

bool Looks(const void* pointer) { return mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(pointer)); }
UInt32 Word(const UInt8* object, UInt32 offset) { return *reinterpret_cast<const UInt32*>(object + offset); }
float Float(const UInt8* object, UInt32 offset) { return *reinterpret_cast<const float*>(object + offset); }
const UInt8* Pointer(const UInt8* object, UInt32 offset) {
	return reinterpret_cast<const UInt8*>(Word(object, offset));
}

const char* NameAt(const UInt8* object) {
	const char* const name = *reinterpret_cast<const char* const*>(object + addr::kNiObjectNameOffset);
	if (!Looks(name)) {
		return "";
	}
	for (UInt32 at = 0; at < 64; ++at) {
		const char c = name[at];
		if (c == '\0') {
			return at > 0 ? name : "";
		}
		if (c < 0x20 || c > 0x7E) {
			return "";
		}
	}
	return "";
}

bool StartsWith(const char* text, const char* prefix) {
	for (UInt32 at = 0; prefix[at] != '\0'; ++at) {
		if (text[at] != prefix[at]) {
			return false;
		}
	}
	return true;
}

bool IsController(const UInt8* object) {
	return Looks(object) && IsControllerClassName(NiClassNameOf(object));
}

// For a flip controller: the texture its target property holds in the
// controller's slot against the one the controller's index names - whether
// the property follows the index. NiFlipController::Update (0x006D1960): the
// textures' array at +0x44 with its count at +0x4A, the index at +0x50, the
// slot at +0x54, the target property at +0x30 with its maps' array at +0x20
// (the count at +0x26), each map's texture at its +0x08 (the setter the flip
// controller calls, 0x00405710, stores it there with the reference counts).
void LogFlipTexture(const char* where, const UInt8* controller) {
	static const char kFlip[] = "NiFlipController@@";
	const char* const className = NiClassNameOf(controller);
	for (UInt32 at = 0; kFlip[at] != '\0'; ++at) {
		if (className[at] != kFlip[at]) {
			return;
		}
	}
	if (g_lines == 0) {
		return;
	}
	const UInt8* const property = Pointer(controller, 0x30);
	const UInt8* const textures = Pointer(controller, 0x44);
	const UInt32 count = *reinterpret_cast<const UInt16*>(controller + 0x4A);
	const UInt32 index = Word(controller, 0x50);
	const UInt32 slot = Word(controller, 0x54);
	UInt32 named = 0;
	if (Looks(textures) && index < count && count <= 64) {
		named = Word(textures, index * 4);
	}
	UInt32 held = 0;
	UInt32 mapCount = 0;
	if (Looks(property)) {
		const UInt8* const maps = Pointer(property, 0x20);
		mapCount = *reinterpret_cast<const UInt16*>(property + 0x26);
		if (Looks(maps) && slot < mapCount && mapCount <= 16) {
			const UInt8* const map = Pointer(maps, slot * 4);
			if (Looks(map)) {
				held = Word(map, 0x08);
			}
		}
	}
	--g_lines;
	OBVR_LOG("Flame probe: %s flip - index %u of %u names texture %08X, the property's map %u of %u holds %08X (%s)",
	         where, index, count, named, slot, mapCount, held,
	         named != 0 && held == named ? "the same" : "NOT the same");
}

void LogController(const char* where, const char* ownerClass, const char* ownerName, const UInt8* controller,
                   UInt32 index) {
	if (StartsWith(where, "first-person")) {
		const char* const cls = NiClassNameOf(controller);
		if (StartsWith(cls, "NiPSysEmitterCtlr")) {
			g_watched[0] = controller;
		} else if (StartsWith(cls, "NiPSysUpdateCtlr")) {
			g_watched[1] = controller;
		}
	}
	if (g_lines == 0) {
		return;
	}
	--g_lines;
	OBVR_LOG("Flame probe: %s %s \"%s\" controller %u %s at %08X - flags %04X, freq %.3f phase %.3f lo %.3f hi %.3f "
	         "start %.3f last %.3f wlast %.3f scaled %.3f, words 2C %08X 30 %08X 34 %08X 38 %08X 3C %08X 40 %08X "
	         "44 %08X 48 %08X 4C %08X 50 %08X 54 %08X",
	         where, ownerClass, ownerName, index, NiClassNameOf(controller), reinterpret_cast<UInt32>(controller),
	         *reinterpret_cast<const UInt16*>(controller + 0x08), static_cast<double>(Float(controller, 0x0C)),
	         static_cast<double>(Float(controller, 0x10)), static_cast<double>(Float(controller, 0x14)),
	         static_cast<double>(Float(controller, 0x18)), static_cast<double>(Float(controller, 0x1C)),
	         static_cast<double>(Float(controller, 0x20)), static_cast<double>(Float(controller, 0x24)),
	         static_cast<double>(Float(controller, 0x28)), Word(controller, 0x2C), Word(controller, 0x30),
	         Word(controller, 0x34), Word(controller, 0x38), Word(controller, 0x3C), Word(controller, 0x40),
	         Word(controller, 0x44), Word(controller, 0x48), Word(controller, 0x4C), Word(controller, 0x50),
	         Word(controller, 0x54));
	LogFlipTexture(where, controller);
}

// The owner's chain of controllers, from its first at +0x0C.
void LogChain(const char* where, const char* ownerClass, const char* ownerName, const UInt8* owner) {
	const UInt8* controller = Pointer(owner, kControllerOffset);
	for (UInt32 index = 0; index < kChainMost && IsController(controller); ++index) {
		LogController(where, ownerClass, ownerName, controller, index);
		const UInt8* const at2C = Pointer(controller, 0x2C);
		const UInt8* const at30 = Pointer(controller, 0x30);
		const UInt8* const at34 = Pointer(controller, 0x34);
		const UInt32 next = NextControllerOffset(IsController(at2C), IsController(at30), IsController(at34));
		if (next == 0) {
			break;
		}
		controller = next == 0x34 ? at34 : next == 0x2C ? at2C : at30;
	}
}

void Walk(const char* where, const UInt8* object, UInt32 depth) {
	if (!Looks(object) || depth > kDepthMost || g_lines == 0) {
		return;
	}
	const char* const className = NiClassNameOf(object);
	const char* const name = NameAt(object);
	--g_lines;
	if (StartsWith(where, "first-person") && StartsWith(name, "FireParticles-Emitter")) {
		g_billboard = object;
		OBVR_LOG("Flame probe: the particles' billboard keeps the saved time %.3f, flags +DC %04X",
		         static_cast<double>(Float(object, 0xE0)), *reinterpret_cast<const UInt16*>(object + 0xDC));
	}
	OBVR_LOG("Flame probe: %s %u %s \"%s\" flags %04X at %08X", where, depth, className, name,
	         *reinterpret_cast<const UInt16*>(object + addr::kNiFlagsOffset), reinterpret_cast<UInt32>(object));
	LogChain(where, className, name, object);
	// The properties, for the flipbook on a texturing property and whatever
	// else is animated there.
	const UInt8* node = Pointer(object, kPropertyListHeadOffset);
	for (UInt32 at = 0; at < kPropertiesMost && Looks(node); ++at) {
		const UInt8* const property = Pointer(node, kListNodeDataOffset);
		if (Looks(property)) {
			LogChain(where, NiClassNameOf(property), name, property);
		}
		node = Pointer(node, kListNodeNextOffset);
	}
	if (!NiClassIsNode(className)) {
		return;
	}
	const UInt8* const* const children = *reinterpret_cast<const UInt8* const* const*>(object + addr::kNiChildrenOffset);
	if (!Looks(children)) {
		return;
	}
	const UInt32 count = *reinterpret_cast<const UInt16*>(object + addr::kNiChildCountOffset);
	if (count > 512) {
		return;
	}
	for (UInt32 at = 0; at < count; ++at) {
		if (Looks(children[at])) {
			Walk(where, children[at], depth + 1);
		}
	}
}

}  // namespace

void ProbeFlames(const char* mark) {
	if (g_calls >= kCallsMost) {
		return;
	}
	++g_calls;
	g_lines = kLinesMost;
	NiAVObject* const firstPerson = FindFirstPersonNode("Torch");
	NiAVObject* bodyTorch = nullptr;
	NiAVObject* const root = PlayerBodyRoot();
	if (root != nullptr) {
		NiAVObject* found[4] = {};
		if (CollectNodesContaining(root, "Torch", found, 4) > 0) {
			bodyTorch = found[0];
		}
	}
	OBVR_LOG("Flame probe: at \"%s\" - the first-person Torch %08X, the body's Torch %08X (each object's class, "
	         "name, flags; each time controller's flags +08, floats +0C..+28, words +2C..+54)",
	         mark != nullptr ? mark : "", reinterpret_cast<UInt32>(firstPerson), reinterpret_cast<UInt32>(bodyTorch));
	if (firstPerson != nullptr) {
		Walk("first-person", reinterpret_cast<const UInt8*>(firstPerson), 0);
	}
	if (bodyTorch != nullptr) {
		Walk("body", reinterpret_cast<const UInt8*>(bodyTorch), 0);
	}
	g_watchFrames = 4;
}

void ProbeFlameTimes(const char* when, bool frameEnds) {
	if (g_watchFrames == 0) {
		return;
	}
	const float frameSeconds = *reinterpret_cast<const float*>(addr::kFrameSecondsAddress);
	for (UInt32 at = 0; at < 2; ++at) {
		const UInt8* const controller = g_watched[at];
		if (!IsController(controller)) {
			continue;
		}
		OBVR_LOG("Flame times: %s - %s last %.3f wlast %.3f scaled %.3f, the billboard's saved time %.3f (frame "
		         "seconds %.5f)", when, at == 0 ? "emitter" : "update", static_cast<double>(Float(controller, 0x20)),
		         static_cast<double>(Float(controller, 0x24)), static_cast<double>(Float(controller, 0x28)),
		         static_cast<double>(Looks(g_billboard) ? Float(g_billboard, 0xE0) : -1.0f),
		         static_cast<double>(frameSeconds));
	}
	if (frameEnds) {
		--g_watchFrames;
	}
}

}  // namespace obvr::game
