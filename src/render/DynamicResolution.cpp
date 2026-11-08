#include "render/DynamicResolution.h"

#include "core/Log.h"

namespace obvr::render {

namespace {

dynres::Settings g_settings;
dynres::State g_state;
std::uint32_t g_moves = 0;
std::uint32_t g_linesLeft = 40;  // moves logged in full; after them one line in twenty

}  // namespace

void ConfigureDynamicResolution(bool enabled, float minScale, float targetShare) {
	g_settings.enabled = enabled;
	g_settings.minScale = dynres::ClampMinScale(minScale);
	g_settings.targetShare = dynres::ClampTargetShare(targetShare);
	if (!enabled) {
		g_state = dynres::State{};
	}
	OBVR_LOG("Dynamic resolution: %s - the world drawn at %.2f to 1.00 of the eye, the GPU's budget %.0f %% of the headset's period",
	         enabled ? "on" : "off", static_cast<double>(g_settings.minScale), static_cast<double>(g_settings.targetShare) * 100.0);
}

void UpdateDynamicResolution(float gpuMs, float periodMs) {
	const float before = g_state.scale;
	const dynres::Move move = dynres::Update(g_state, g_settings, gpuMs, periodMs);
	if (move == dynres::Move::kNone) {
		return;
	}
	++g_moves;
	if (g_linesLeft > 0 || g_moves % 20 == 0) {
		if (g_linesLeft > 0) {
			--g_linesLeft;
		}
		OBVR_LOG("Dynamic resolution: scale %.3f -> %.3f (%s) - the last frame's GPU %.2f ms of a %.2f ms budget (period %.2f ms); move %u",
		         static_cast<double>(before), static_cast<double>(g_state.scale),
		         move == dynres::Move::kDown ? "down" : move == dynres::Move::kUp ? "up" : "reset", static_cast<double>(gpuMs),
		         static_cast<double>(periodMs * g_settings.targetShare), static_cast<double>(periodMs), g_moves);
	}
}

bool DynamicResolutionActive() {
	return g_settings.enabled && g_state.scale < dynres::kMaxScale;
}

dynres::Layout DynamicResolutionLayout(std::uint32_t frameWidth, std::uint32_t frameHeight) {
	return dynres::LayoutFor(frameWidth, frameHeight, g_settings.enabled ? g_state.scale : 1.0f);
}

float DynamicResolutionScale() {
	return g_settings.enabled ? g_state.scale : 1.0f;
}

}  // namespace obvr::render
