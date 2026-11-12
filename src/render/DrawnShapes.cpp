#include "render/DrawnShapes.h"

#include <cstring>
#include <unordered_set>

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "game/GameTypes.h"

namespace obvr::render {
namespace {

constexpr UInt32 kRendererVtable = 0x00A88EA4;
constexpr UInt32 kRenderShapeSlots[2] = {88, 89};
constexpr UInt32 kRenderShape = 0x007693E0;
constexpr UInt32 kWaterCullingProcessVtable = 0x00A3F380;

// Do_RenderShape is __thiscall(geometry), RET 4: a __fastcall with the dead
// edx slot is the same call.
using RenderShapeFn = UInt32(__fastcall*)(void* renderer, void* edx, void* geometry);

struct State {
	RenderShapeFn original = nullptr;
	bool installed = false;
	bool refused = false;
	bool recording = false;
	bool world = false;
	UInt32 holds = 0;
	UInt32 held = 0;
	std::unordered_set<const void*> inWorld;
	std::unordered_set<const void*> elsewhere;
	std::unordered_set<const void*> otherCull;
	const NiAVObject* paperdoll = nullptr;
	UInt32 paperdollDraws = 0;
	void (*paperdollSeen)(const void* model) = nullptr;
};

State& S() {
	static State s;
	return s;
}

// The menu's figure above a shape, while it is drawn: the engine hangs the
// model on its holder for the draw and takes it off again (read before the
// next world render, the holder had no child: finery-checks 2026-11-12), so
// it is found here, each step up checked to be a live game object.
const NiAVObject* PaperdollAbove(const void* geometry) {
	const auto* node = static_cast<const NiAVObject*>(geometry);
	for (UInt32 depth = 0; depth < 24; ++depth) {
		const UInt32 at = reinterpret_cast<UInt32>(node);
		if (!mem::LooksLikeObjectAddress(at) || !addr::IsGameVtable(*reinterpret_cast<const UInt32*>(at))) {
			return nullptr;
		}
		const char* const name = node->name;
		if (mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(name)) && std::strcmp(name, "PlayerInventoryPC") == 0) {
			return node;
		}
		node = node->parent;
	}
	return nullptr;
}

UInt32 __fastcall RecordShape(void* renderer, void* edx, void* geometry) {
	State& s = S();
	if (s.recording && geometry != nullptr) {
		if (s.holds > 0) {
			++s.held;
			s.otherCull.insert(geometry);
		} else if (s.world) {
			s.inWorld.insert(geometry);
		} else {
			s.elsewhere.insert(geometry);
		}
		if (s.holds > 0 || !s.world) {
			if (const NiAVObject* figure = PaperdollAbove(geometry)) {
				const bool first = s.paperdoll == nullptr;
				s.paperdoll = figure;
				++s.paperdollDraws;
				if (first && s.paperdollSeen != nullptr) {
					s.paperdollSeen(figure);
				}
			}
		}
	}
	return s.original(renderer, edx, geometry);
}

}  // namespace

bool InstallDrawnShapes() {
	State& s = S();
	if (s.installed || s.refused) {
		return s.installed;
	}
	for (const UInt32 slot : kRenderShapeSlots) {
		const UInt32 at = kRendererVtable + slot * 4;
		const UInt32 expected = kRenderShape;
		if (!mem::Verify(at, reinterpret_cast<const UInt8*>(&expected), sizeof(expected))) {
			OBVR_LOG("Wear check: the renderer's slot %u (%08X) does not hold Do_RenderShape %08X - the draws are not "
			         "recorded",
			         slot, at, kRenderShape);
			s.refused = true;
			return false;
		}
	}
	s.original = reinterpret_cast<RenderShapeFn>(kRenderShape);
	const UInt32 recorder = reinterpret_cast<UInt32>(&RecordShape);
	for (const UInt32 slot : kRenderShapeSlots) {
		if (!mem::SafeWrite(kRendererVtable + slot * 4, &recorder, sizeof(recorder))) {
			OBVR_LOG("Wear check: the renderer's slot %u could not be written - the draws are not recorded", slot);
			s.refused = true;
			return false;
		}
	}
	s.installed = true;
	OBVR_LOG("Wear check: the renderer's shape draws (slots 88 and 89 of %08X) are recorded at the marks",
	         kRendererVtable);
	return true;
}

void BeginDrawnShapes() {
	State& s = S();
	s.inWorld.clear();
	s.elsewhere.clear();
	s.otherCull.clear();
	s.paperdoll = nullptr;
	s.paperdollDraws = 0;
	s.held = 0;
	s.holds = 0;
	s.recording = s.installed;
}

void EndDrawnShapes() { S().recording = false; }

bool DrawnShapesRecording() { return S().recording; }

void SetDrawnShapesWorld(bool inside) { S().world = inside; }

bool IsEyeCull(const void* process, const void* camera) {
	const UInt32 graph = *reinterpret_cast<const UInt32*>(addr::kWorldSceneGraphPointer);
	if (!mem::LooksLikeObjectAddress(graph) || !mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(process))) {
		return false;
	}
	const UInt32 main = *reinterpret_cast<const UInt32*>(graph + addr::kSceneGraphCameraOffset);
	const UInt32 vtable = *reinterpret_cast<const UInt32*>(process);
	return reinterpret_cast<UInt32>(camera) == main && vtable != kWaterCullingProcessVtable;
}

DrawnShapesHold::DrawnShapesHold(const void* process, const void* camera) {
	State& s = S();
	if (s.recording && s.world && !IsEyeCull(process, camera)) {
		++s.holds;
		m_held = true;
	}
}

DrawnShapesHold::~DrawnShapesHold() {
	State& s = S();
	if (m_held && s.holds > 0) {
		--s.holds;
	}
}

bool DrawnInWorld(const void* geometry) { return S().inWorld.count(geometry) != 0; }
bool DrawnElsewhere(const void* geometry) { return S().elsewhere.count(geometry) != 0; }
bool DrawnInOtherCull(const void* geometry) { return S().otherCull.count(geometry) != 0; }
const void* DrawnPaperdoll() { return S().paperdoll; }
void SetDrawnPaperdollSeen(void (*seen)(const void* model)) { S().paperdollSeen = seen; }
UInt32 DrawnPaperdollDraws() { return S().paperdollDraws; }
UInt32 DrawnWorldCount() { return static_cast<UInt32>(S().inWorld.size()); }
UInt32 DrawnElsewhereCount() { return static_cast<UInt32>(S().elsewhere.size()); }
UInt32 DrawnHeldCount() { return S().held; }

}  // namespace obvr::render
