#include "game/TargetMarker.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"

namespace obvr::game {
namespace {

constexpr UInt32 kLookupFormById = 0x0046B250;  // xOBSE GameAPI.cpp, cdecl, as HandBones.cpp
constexpr UInt32 kGameAlloc = 0x00401F00;       // cdecl(size)
constexpr UInt32 kEffectSize = 0x4C;
constexpr UInt32 kMagicShaderHitEffectCtor = 0x006A0980;
constexpr UInt32 kEffectInitSlot = 0x68;
constexpr UInt32 kAddTempEffect = 0x00678D30;
constexpr UInt32 kStopShaderOnRef = 0x00678E70;
constexpr UInt32 kRef3DSlot = 0x154;

bool LooksLikeObject(UInt32 address) { return mem::LooksLikeObjectAddress(address); }
bool LooksLikeObject(const void* pointer) { return LooksLikeObject(reinterpret_cast<UInt32>(pointer)); }

MarkState g_mark;
bool g_outlineOn = false;
bool g_glowOn = false;
UInt32 g_lines = 16;

void* Form(UInt32 id) {
	using LookupFn = void*(__cdecl*)(UInt32 id);
	void* const form = reinterpret_cast<LookupFn>(kLookupFormById)(id);
	return LooksLikeObject(form) ? form : nullptr;
}

bool HasThreeD(UInt32 ref) {
	if (!LooksLikeObject(ref)) {
		return false;
	}
	const UInt32 vtable = *reinterpret_cast<const UInt32*>(ref);
	if (!LooksLikeObject(vtable)) {
		return false;
	}
	using NodeFn = void*(__thiscall*)(void* ref);
	const UInt32 fn = *reinterpret_cast<const UInt32*>(vtable + kRef3DSlot);
	return fn != 0 && reinterpret_cast<NodeFn>(fn)(reinterpret_cast<void*>(ref)) != nullptr;
}

bool Start(UInt32 ref, UInt32 shaderId) {
	void* const shader = Form(shaderId);
	if (shader == nullptr || !HasThreeD(ref)) {
		return false;
	}
	using AllocFn = void*(__cdecl*)(UInt32 size);
	void* const effect = reinterpret_cast<AllocFn>(kGameAlloc)(kEffectSize);
	if (effect == nullptr) {
		return false;
	}
	using CtorFn = void*(__thiscall*)(void* self, void* target, void* shader, float duration);
	reinterpret_cast<CtorFn>(kMagicShaderHitEffectCtor)(effect, reinterpret_cast<void*>(ref), shader, -1.0f);
	const UInt32 vtable = *reinterpret_cast<const UInt32*>(effect);
	using InitFn = bool(__thiscall*)(void* self);
	using DeleteFn = void(__thiscall*)(void* self, UInt32 flags);
	if (!reinterpret_cast<InitFn>(*reinterpret_cast<const UInt32*>(vtable + kEffectInitSlot))(effect)) {
		reinterpret_cast<DeleteFn>(*reinterpret_cast<const UInt32*>(vtable))(effect, 1);
		return false;
	}
	using AddFn = void(__thiscall*)(void* manager, void* effect);
	reinterpret_cast<AddFn>(kAddTempEffect)(reinterpret_cast<void*>(addr::kActorProcessManager), effect);
	return true;
}

void Stop(UInt32 ref, UInt32 shaderId) {
	void* const shader = Form(shaderId);
	if (shader == nullptr || !LooksLikeObject(ref)) {
		return;
	}
	using StopFn = void(__thiscall*)(void* manager, void* ref, void* shader);
	reinterpret_cast<StopFn>(kStopShaderOnRef)(reinterpret_cast<void*>(addr::kActorProcessManager),
	                                           reinterpret_cast<void*>(ref), shader);
}

bool Verified() {
	static int s_ok = -1;
	if (s_ok < 0) {
		// The constructor's and the manager's entries as read (push ebp / sub
		// esp or the like differ; the first bytes are checked against what the
		// file holds at those addresses, and a foreign hook refuses).
		const UInt8 ctorFirst = *reinterpret_cast<const UInt8*>(kMagicShaderHitEffectCtor);
		const UInt8 addFirst = *reinterpret_cast<const UInt8*>(kAddTempEffect);
		const UInt8 stopFirst = *reinterpret_cast<const UInt8*>(kStopShaderOnRef);
		s_ok = (ctorFirst != 0xE9 && addFirst != 0xE9 && stopFirst != 0xE9) ? 1 : 0;
		OBVR_LOG("Target marker: %s", s_ok ? "the engine's effect shader calls in place"
		                                   : "a hook sits on the effect calls - no marks");
	}
	return s_ok == 1;
}

}  // namespace

void StepTargetMarker(UInt32 ref, bool outline, bool glow) {
	if (!Verified()) {
		return;
	}
	// A mark switched off comes off at once.
	if (g_mark.marked != 0) {
		if (g_outlineOn && !outline) {
			Stop(g_mark.marked, kShaderOutline);
			g_outlineOn = false;
		}
		if (g_glowOn && !glow) {
			Stop(g_mark.marked, kShaderGlow);
			g_glowOn = false;
		}
	}
	const MarkStep step = StepMark(g_mark, (outline || glow) ? ref : 0);
	if (step.stop != 0) {
		if (g_outlineOn) {
			Stop(step.stop, kShaderOutline);
		}
		if (g_glowOn) {
			Stop(step.stop, kShaderGlow);
		}
		g_outlineOn = g_glowOn = false;
	}
	if (g_mark.marked != 0) {
		if (outline && !g_outlineOn) {
			g_outlineOn = Start(g_mark.marked, kShaderOutline);
		}
		if (glow && !g_glowOn) {
			g_glowOn = Start(g_mark.marked, kShaderGlow);
		}
	}
	if (step.start != 0 && !g_outlineOn && !g_glowOn && g_lines > 0) {
		void* const outlineForm = Form(kShaderOutline);
		void* const glowForm = Form(kShaderGlow);
		OBVR_LOG("Target marker: %08X could not be marked - the outline shader %08X %s, the glow %08X %s, its 3D %s",
		         step.start, kShaderOutline, outlineForm ? "found" : "NOT found", kShaderGlow,
		         glowForm ? "found" : "NOT found", HasThreeD(step.start) ? "there" : "missing");
	}
	if ((step.start != 0 || step.stop != 0) && g_lines > 0) {
		--g_lines;
		OBVR_LOG("Target marker: %08X marked (outline %d, glow %d), %08X unmarked", step.start, g_outlineOn ? 1 : 0,
		         g_glowOn ? 1 : 0, step.stop);
	}
}

}  // namespace obvr::game
