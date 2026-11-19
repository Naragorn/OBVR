#include "render/FirstPersonPass.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/GameAddresses.h"
#include "render/BackfacePass.h"
#include "render/SceneRenderHook.h"

namespace obvr::render {

bool InFirstPersonPass() {
	static AccumulatorWatch s_watch;
	const UInt8* const renderer = *reinterpret_cast<const UInt8* const*>(addr::kRendererPointer);
	const bool rendererOk = mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(renderer));
	const UInt8* const accumulator =
		rendererOk ? *reinterpret_cast<const UInt8* const*>(renderer + addr::kRendererAccumulatorOffset) : nullptr;
	const UInt32 at = reinterpret_cast<UInt32>(accumulator);
	const bool running = SceneRenderRunning();
	if (!MayReadFirstPersonByte(running, rendererOk, mem::LooksLikeObjectAddress(at))) {
		if (!running && rendererOk && s_watch.LogOutside(at)) {
			OBVR_LOG("Closed hands: outside the scene render the renderer's accumulator is %08X, the scene "
			         "render's was %08X - its first-person byte is not read there", at, s_watch.scene);
		}
		return false;
	}
	s_watch.NoteScene(at);
	return accumulator[kAccumulatorFirstPersonOffset] != 0;
}

}  // namespace obvr::render
