#include "render/ShapeTurnHook.h"

#include "core/AddressSpace.h"
#include "core/Config.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/BillboardTime.h"
#include "game/GameTypes.h"
#include "game/ParticlesAfterPins.h"

namespace obvr::render {
namespace {

// The renderer's draw calls for one shape, __thiscall with `ret 4`: the same
// call as __fastcall with a dead edx.
using DrawShapeFn = void(__fastcall*)(void* renderer, void* unusedEdx, void* shape);

constexpr UInt32 kRendererVtable = 0x00A88EA4;
constexpr UInt32 kFirstSlot = 88;
constexpr int kSlotCount = 7;
constexpr UInt32 kFunctions[kSlotCount] = {0x007693E0, 0x007693E0, 0x007694F0, 0x00769700,
                                           0x00765990, 0x00766880, 0x0076B160};
constexpr UInt32 kTraceDraws = 6;

struct State {
	DrawShapeFn original[kSlotCount] = {};
	ShapeTurn turn = ShapeTurn::Unknown;
	const void* shape = nullptr;
	int slot = -1;
	UInt32 traceLeft[kSlotCount] = {kTraceDraws, kTraceDraws, kTraceDraws, kTraceDraws,
	                                kTraceDraws, kTraceDraws, kTraceDraws};
};

State& S() {
	static State s;
	return s;
}

ShapeTurn TurnOf(const void* shape) {
	if (!mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(shape))) {
		return ShapeTurn::Unknown;
	}
	const NiTransform& world = static_cast<const NiAVObject*>(shape)->worldTransform;
	return ShapeTurnOf(world.rot, world.scale);
}

template <int K>
void __fastcall Hooked(void* renderer, void* unusedEdx, void* shape) {
	State& s = S();
	const ShapeTurn turn = s.turn;
	const void* const was = s.shape;
	const int slot = s.slot;
	s.turn = TurnOf(shape);
	s.shape = shape;
	s.slot = K;
	s.original[K](renderer, unusedEdx, shape);
	s.turn = turn;
	s.shape = was;
	s.slot = slot;
}

constexpr DrawShapeFn kHooks[kSlotCount] = {&Hooked<0>, &Hooked<1>, &Hooked<2>, &Hooked<3>,
                                            &Hooked<4>, &Hooked<5>, &Hooked<6>};

UInt32 SlotAddress(int k) { return kRendererVtable + (kFirstSlot + static_cast<UInt32>(k)) * 4; }

}  // namespace

void InstallShapeTurnHook() {
	State& s = S();
	if (s.original[0] != nullptr) {
		return;
	}
	for (int k = 0; k < kSlotCount; ++k) {
		const UInt32 held = *reinterpret_cast<const UInt32*>(SlotAddress(k));
		if (held != kFunctions[k]) {
			OBVR_LOG("Mirror cull: the renderer's slot %u (%08X) holds %08X, not %08X as read - left alone; under "
			         "the mirror every first-person draw culls the other way (a torch's flame goes out)",
			         kFirstSlot + k, SlotAddress(k), held, kFunctions[k]);
			mem::ReportForeignCode("Mirror cull", SlotAddress(k));
			return;
		}
	}
	for (int k = 0; k < kSlotCount; ++k) {
		s.original[k] = reinterpret_cast<DrawShapeFn>(kFunctions[k]);
		const UInt32 hook = reinterpret_cast<UInt32>(kHooks[k]);
		if (!mem::SafeWrite(SlotAddress(k), &hook, sizeof(hook))) {
			OBVR_LOG("Mirror cull: the write to the renderer's slot %u failed", kFirstSlot + k);
		}
	}
	OBVR_LOG("Mirror cull: the renderer's shape draws (slots 88 to 94 of %08X) note each shape's turn - under the "
	         "mirror only a turned-over shape culls the other way",
	         kRendererVtable);
}

void InstallFirstPersonEffectHooks(const Config& config) {
	game::InstallBillboardTime(config.keepBillboardTime);
	game::InstallParticlesAfterPins(config.firstPersonParticlesAfterPins);
	InstallShapeTurnHook();
}

ShapeTurn CurrentShapeTurn() {
	State& s = S();
	if (s.slot >= 0 && s.traceLeft[s.slot] > 0) {
		--s.traceLeft[s.slot];
		const char* name = static_cast<const NiAVObject*>(s.shape)->name;
		const bool named = mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(name));
		OBVR_LOG("Mirror cull: slot %u draws \"%.40s\" (%08X) in the mirrored first-person pass - %s",
		         kFirstSlot + s.slot, named ? name : "?", reinterpret_cast<UInt32>(s.shape),
		         s.turn == ShapeTurn::TurnedOver ? "turned over, the cull swaps"
		         : s.turn == ShapeTurn::Upright  ? "upright, the engine's cull"
		                                         : "not known, the cull swaps");
	}
	return s.turn;
}

bool IsShapeTurnHook(UInt32 function) {
	for (const DrawShapeFn hook : kHooks) {
		if (reinterpret_cast<UInt32>(hook) == function) {
			return true;
		}
	}
	return false;
}

}  // namespace obvr::render
