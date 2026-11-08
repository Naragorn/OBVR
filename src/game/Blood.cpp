#include "game/Blood.h"

#include "game/CutMarks.h"
#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"

namespace obvr::game {
namespace {

constexpr UInt32 kBleeds = 0x005E1B30;       // Actor, thiscall() -> bool: bleeds at all
constexpr UInt32 kSprayPath = 0x005E1BF0;    // Actor, thiscall() -> const char*
constexpr UInt32 kDecalPath = 0x005E1BB0;    // Actor, thiscall() -> const char*
constexpr UInt32 kQuadOf = 0x004C9BE0;       // cdecl(actor) -> char
constexpr UInt32 kEffectNode = 0x00441800;   // TESObjectCELL, thiscall(quad, 3) -> NiNode*
constexpr UInt32 kHeapAlloc = 0x00401F00;    // cdecl(size) -> void*
constexpr UInt32 kParticleCtor = 0x005713F0; // BSTempEffectParticle, thiscall(...), ret 0x30
constexpr UInt32 kEffectManager = 0x00B3BD00;
constexpr UInt32 kManagerAdd = 0x00678D30;   // thiscall(effect), ret 4; null-safe
constexpr UInt32 kCellDecal = 0x004D3B10;    // TESObjectCELL, thiscall(10 dwords), ret 0x28

const UInt8 kGetterBytes[] = {0x53, 0x56, 0x8B, 0xF1, 0x8B, 0x06, 0x8B, 0x90};
const UInt8 kQuadBytes[] = {0x56, 0x8B, 0x74, 0x24, 0x08, 0x57, 0x33, 0xFF};
const UInt8 kEffectNodeBytes[] = {0x56, 0x8B, 0x74, 0x24, 0x08, 0x83, 0xC6, 0x02};
const UInt8 kAllocBytes[] = {0x8B, 0x44, 0x24, 0x04, 0x6A, 0x01, 0x50, 0xB9};
const UInt8 kCtorBytes[] = {0x6A, 0xFF, 0x68, 0x43, 0xDD, 0x9B, 0x00, 0x64};
const UInt8 kAddBytes[] = {0x56, 0x8B, 0x74, 0x24, 0x08, 0x85, 0xF6, 0x57};
const UInt8 kDecalBytes[] = {0x6A, 0xFF, 0x68, 0xAB, 0x56, 0x9B, 0x00, 0x64};

using BoolGetter = bool(__fastcall*)(void* self, void* edx);
using PathGetter = const char*(__fastcall*)(void* self, void* edx);
using QuadFn = UInt8(__cdecl*)(void* actor);
using EffectNodeFn = void*(__fastcall*)(void* cell, void* edx, UInt32 quad, UInt32 kind);
using AllocFn = void*(__cdecl*)(UInt32 size);
using CtorFn = void*(__fastcall*)(void* self, void* edx, void* cell, float lifetime, void* parent, const char* path,
                                  float dx, float dy, float dz, float px, float py, float pz, float scale,
                                  UInt32 shared);
using AddFn = void(__fastcall*)(void* manager, void* edx, void* effect);
using DecalFn = void(__fastcall*)(void* cell, void* edx, float px, float py, float pz, float dx, float dy, float dz,
                                  const char* texture, UInt32 a, SInt32 b, UInt32 c);

bool g_ready = false;
UInt32 g_lines = 12;

bool Bleeds(void* actor) { return reinterpret_cast<BoolGetter>(kBleeds)(actor, nullptr); }

void* CellOf(UInt32 actor) {
	const UInt32 cell = *reinterpret_cast<const UInt32*>(actor + addr::kRefParentCellOffset);
	return mem::LooksLikeObjectAddress(cell) ? reinterpret_cast<void*>(cell) : nullptr;
}

const char* Usable(const char* path) { return path != nullptr && path[0] != '\0' ? path : nullptr; }

}  // namespace

void InstallBlood() {
	g_ready = mem::Verify(kBleeds, kGetterBytes, sizeof(kGetterBytes)) &&
	          mem::Verify(kSprayPath, kGetterBytes, sizeof(kGetterBytes)) &&
	          mem::Verify(kDecalPath, kGetterBytes, sizeof(kGetterBytes)) &&
	          mem::Verify(kQuadOf, kQuadBytes, sizeof(kQuadBytes)) &&
	          mem::Verify(kEffectNode, kEffectNodeBytes, sizeof(kEffectNodeBytes)) &&
	          mem::Verify(kHeapAlloc, kAllocBytes, sizeof(kAllocBytes)) &&
	          mem::Verify(kParticleCtor, kCtorBytes, sizeof(kCtorBytes)) &&
	          mem::Verify(kManagerAdd, kAddBytes, sizeof(kAddBytes)) &&
	          mem::Verify(kCellDecal, kDecalBytes, sizeof(kDecalBytes));
	OBVR_LOG("Blood: the engine's spray and stain %s",
	         g_ready ? "as read - a cut bleeds" : "NOT as read - a cut does not bleed");
}

bool SprayBlood(UInt32 actor, const NiPoint3& at, const NiPoint3& dir) {
	if (!g_ready || !mem::LooksLikeObjectAddress(actor)) {
		return false;
	}
	void* const self = reinterpret_cast<void*>(actor);
	void* const cell = CellOf(actor);
	const char* const path = Bleeds(self) ? Usable(reinterpret_cast<PathGetter>(kSprayPath)(self, nullptr)) : nullptr;
	if (cell == nullptr || path == nullptr) {
		return false;
	}
	const UInt8 quad = reinterpret_cast<QuadFn>(kQuadOf)(self);
	void* const parent = reinterpret_cast<EffectNodeFn>(kEffectNode)(cell, nullptr, quad, 3);
	if (!mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(parent))) {
		if (g_lines > 0) {
			--g_lines;
			OBVR_LOG("Blood: no effect node in %08X's cell (quad %u) - no spray", actor, static_cast<UInt32>(quad));
		}
		return false;
	}
	void* const memory = reinterpret_cast<AllocFn>(kHeapAlloc)(0x20);
	if (memory == nullptr) {
		return false;
	}
	// As the melee hit makes it: lifetime 1 (the model's own controllers
	// decide), scale 1, the model cloned rather than shared.
	void* const effect = reinterpret_cast<CtorFn>(kParticleCtor)(memory, nullptr, cell, 1.0f, parent, path, dir.x, dir.y,
	                                                             dir.z, at.x, at.y, at.z, 1.0f, 0);
	reinterpret_cast<AddFn>(kManagerAdd)(reinterpret_cast<void*>(kEffectManager), nullptr, effect);
	if (g_lines > 0) {
		--g_lines;
		OBVR_LOG("Blood: %s sprayed for %08X at %.0f %.0f %.0f going %.2f %.2f %.2f", path, actor,
		         static_cast<double>(at.x), static_cast<double>(at.y), static_cast<double>(at.z),
		         static_cast<double>(dir.x), static_cast<double>(dir.y), static_cast<double>(dir.z));
	}
	return true;
}

const char* BloodDecalTexture(UInt32 actor) {
	if (!g_ready || !mem::LooksLikeObjectAddress(actor)) {
		return nullptr;
	}
	void* const self = reinterpret_cast<void*>(actor);
	return Bleeds(self) ? Usable(reinterpret_cast<PathGetter>(kDecalPath)(self, nullptr)) : nullptr;
}

bool StainBodyBlood(UInt32 actor, const NiPoint3& at, const NiPoint3& dir) {
	CutMark mark;
	mark.place = true;
	mark.from = at;
	mark.direction = dir;
	mark.texture = BloodDecalTexture(actor);
	if (mark.texture == nullptr) {
		return false;
	}
	char why[160];
	return LayCutMark(actor, mark, NiPoint3{0.0f, 0.0f, 1.0f}, why, sizeof(why));
}

bool StainBlood(UInt32 actor, const NiPoint3& at, const NiPoint3& dir) {
	if (!g_ready || !mem::LooksLikeObjectAddress(actor)) {
		return false;
	}
	void* const self = reinterpret_cast<void*>(actor);
	void* const cell = CellOf(actor);
	const char* const texture =
		Bleeds(self) ? Usable(reinterpret_cast<PathGetter>(kDecalPath)(self, nullptr)) : nullptr;
	if (cell == nullptr || texture == nullptr) {
		return false;
	}
	reinterpret_cast<DecalFn>(kCellDecal)(cell, nullptr, at.x, at.y, at.z, dir.x, dir.y, dir.z, texture, 0, -1, 0);
	if (g_lines > 0) {
		--g_lines;
		OBVR_LOG("Blood: %s laid for %08X from %.0f %.0f %.0f going %.2f %.2f %.2f", texture, actor,
		         static_cast<double>(at.x), static_cast<double>(at.y), static_cast<double>(at.z),
		         static_cast<double>(dir.x), static_cast<double>(dir.y), static_cast<double>(dir.z));
	}
	return true;
}

}  // namespace obvr::game
