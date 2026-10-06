#include "game/SceneFrame.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"

namespace obvr::game {
namespace {

bool g_verified = false;
bool g_usable = false;
UInt32 g_lines = 6;

// The engine's own close (0x007D7210: push esi; mov esi,[0x00B3F928]; call
// 0x007D7150; cmp [esi+0x200],1), and the copy shader's gate on that state.
bool Verify() {
	if (g_verified) {
		return g_usable;
	}
	g_verified = true;
	static const UInt8 kClose[13] = {0x56, 0x8B, 0x35, 0x28, 0xF9, 0xB3, 0x00, 0xE8, 0x34, 0xFF, 0xFF, 0xFF, 0x83};
	static const UInt8 kGate[7] = {0x83, 0xB8, 0x00, 0x02, 0x00, 0x00, 0x00};
	static const UInt8 kPop[5] = {0xA1, 0x74, 0x5D, 0xB4, 0x00};
	g_usable = mem::Verify(kRendererEndAndDisplayFrame, kClose, sizeof(kClose)) &&
	           mem::Verify(kCopyShaderFrameGate, kGate, sizeof(kGate)) &&
	           mem::Verify(kPopRenderTargetGroups, kPop, sizeof(kPop));
	if (!g_usable) {
		OBVR_LOG("Dual pass: the engine's frame close (%08X), copy gate (%08X) or group pop (%08X) is not as read - "
		         "without antialiasing both eyes will get the same picture",
		         kRendererEndAndDisplayFrame, kCopyShaderFrameGate, kPopRenderTargetGroups);
		mem::ReportForeignCode("Dual pass", kRendererEndAndDisplayFrame);
	}
	return g_usable;
}

}  // namespace

bool CloseSceneFrameBetweenPasses() {
	if (!Verify()) {
		return false;
	}
	const UInt32 renderer = *reinterpret_cast<const UInt32*>(addr::kRendererPointer);
	if (!mem::LooksLikeObjectAddress(renderer)) {
		return false;
	}
	UInt32* const frameState = reinterpret_cast<UInt32*>(renderer + kRendererFrameStateOffset);
	const UInt32 offscreenState = *reinterpret_cast<const UInt32*>(renderer + kRendererOffscreenStateOffset);
	const bool imageSpace = *reinterpret_cast<const UInt8*>(kImageSpaceEffectsByte) != 0;
	const UInt32 samples = *reinterpret_cast<const UInt32*>(kEffectiveSampleCount);
	if (!SceneFrameCloseWanted(imageSpace, samples, *frameState, offscreenState)) {
		if (g_lines > 0 && imageSpace && samples < 2) {
			--g_lines;
			OBVR_LOG("Dual pass: the frame between the passes reads state %u/%u on the texture path - not closed",
			         *frameState, offscreenState);
		}
		return false;
	}
	const UInt32 vtable = *reinterpret_cast<const UInt32*>(renderer);
	const UInt32 endFrame = mem::LooksLikeObjectAddress(vtable)
	                            ? *reinterpret_cast<const UInt32*>(vtable + kRendererEndFrameSlot)
	                            : 0;
	if (endFrame != kRendererEndFrame) {
		if (g_lines > 0) {
			--g_lines;
			OBVR_LOG("Dual pass: the renderer's EndFrame slot holds %08X, not %08X - the frame is left open",
			         endFrame, kRendererEndFrame);
		}
		return false;
	}
	using PopFn = void(__cdecl*)();
	using EndFrameFn = bool(__thiscall*)(void* renderer);
	reinterpret_cast<PopFn>(kPopRenderTargetGroups)();
	const bool ended = reinterpret_cast<EndFrameFn>(endFrame)(reinterpret_cast<void*>(renderer));
	if (!ended) {
		if (g_lines > 0) {
			--g_lines;
			OBVR_LOG("Dual pass: EndFrame refused between the passes - the frame is left open");
		}
		return false;
	}
	*frameState = 0;
	if (g_lines > 0) {
		--g_lines;
		OBVR_LOG("Dual pass: the frame closed between the passes (texture path, %u sample(s)) - the second eye's "
		         "copy into the back buffer can run",
		         samples);
	}
	return true;
}

}  // namespace obvr::game
