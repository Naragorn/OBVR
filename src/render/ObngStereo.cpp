#include "render/ObngStereo.h"

#include "core/Log.h"
#include "platform/Win32Min.h"

namespace obvr::render {
namespace {

using PlanFn = std::uint32_t(__cdecl*)();
using SetEyesFn = std::uint32_t(__cdecl*)(const ObngEyes* eyes);
using ClearEyesFn = void(__cdecl*)();

// OBNG.dll's three functions, and whether the look is over (bound, or
// OBNG.dll found without all three).
struct Binding {
	PlanFn plan = nullptr;
	SetEyesFn setEyes = nullptr;
	ClearEyesFn clearEyes = nullptr;
	bool done = false;
};
Binding g_obng;

}  // namespace

bool BindObngStereo() {
	if (g_obng.done) {
		return g_obng.plan != nullptr && g_obng.setEyes != nullptr && g_obng.clearEyes != nullptr;
	}
	const HMODULE obng = GetModuleHandleA("OBNG.dll");
	if (obng == nullptr) {
		return false;  // not loaded (yet): asked again next time
	}
	g_obng.done = true;
	g_obng.plan = reinterpret_cast<PlanFn>(GetProcAddress(obng, "ObngStereoPlan"));
	g_obng.setEyes = reinterpret_cast<SetEyesFn>(GetProcAddress(obng, "ObngStereoSetEyes"));
	g_obng.clearEyes = reinterpret_cast<ClearEyesFn>(GetProcAddress(obng, "ObngStereoClearEyes"));
	const bool all = g_obng.plan != nullptr && g_obng.setEyes != nullptr && g_obng.clearEyes != nullptr;
	OBVR_LOG("Single-pass stereo: OBNG.dll found - ObngStereoPlan %s, ObngStereoSetEyes %s, ObngStereoClearEyes %s%s",
	         g_obng.plan != nullptr ? "bound" : "missing", g_obng.setEyes != nullptr ? "bound" : "missing",
	         g_obng.clearEyes != nullptr ? "bound" : "missing",
	         all ? "" : " - the dual pass stays");
	if (!all) {
		g_obng.plan = nullptr;
		g_obng.setEyes = nullptr;
		g_obng.clearEyes = nullptr;
	}
	return all;
}

std::uint32_t ObngPlan() {
	return g_obng.plan != nullptr ? g_obng.plan() : 0u;
}

std::uint32_t ObngSetEyes(const ObngEyes& eyes) {
	return g_obng.setEyes != nullptr ? g_obng.setEyes(&eyes) : kObngNotBound;
}

void ObngClearEyes() {
	if (g_obng.clearEyes != nullptr) {
		g_obng.clearEyes();
	}
}

}  // namespace obvr::render
