#include "render/DeviceState.h"

#include "core/Log.h"
#include "render/D3D9Types.h"

namespace obvr::render {
namespace {

// One refusal is permanent. If the device will not make a state block once it
// will not make one later, and a log line per frame saying so would bury
// everything else.
bool g_refused = false;
bool g_reported = false;

}  // namespace

void* CaptureDeviceState(void* device) {
	if (device == nullptr || g_refused) {
		return nullptr;
	}

	auto create = d3d9::Method<d3d9::CreateStateBlockFn>(device, d3d9::kDeviceCreateStateBlock);
	void* block = nullptr;
	if (create == nullptr || create(device, d3d9::kStateBlockTypeAll, &block) < 0 ||
	    block == nullptr) {
		g_refused = true;
		OBVR_LOG("Render: no state block - the two world renders will start from whatever "
		         "state the previous one left, as they did before");
		return nullptr;
	}

	if (!g_reported) {
		g_reported = true;
		OBVR_LOG("Render: the second world render now starts from the same device state as "
		         "the first");
	}

	// Creating a block captures the current state as part of creating it, so
	// there is nothing left to ask for here.
	return block;
}

void RestoreDeviceState(void* state) {
	if (state == nullptr) {
		return;
	}
	if (auto apply = d3d9::Method<d3d9::StateBlockMethodFn>(state, d3d9::kStateBlockApply)) {
		apply(state);
	}
	using ReleaseFn = UInt32(__stdcall*)(void*);
	if (auto release = d3d9::Method<ReleaseFn>(state, 2)) {
		release(state);
	}
}

}  // namespace obvr::render
