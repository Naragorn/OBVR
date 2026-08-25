#include "render/GameDevice.h"

#include "game/GameAddresses.h"
#include "render/D3D11Types.h"

namespace obvr::render {

const Guid kIID_D3D9VkInteropDevice = {
	0x2eaa4b89, 0x0107, 0x4bdb, {0x87, 0xf7, 0x0f, 0x54, 0x1c, 0x49, 0x3c, 0xe0}};

const Guid kIID_D3D9VkInteropTexture = {
	0xd56344f5, 0x8d35, 0x46fd, {0x80, 0x6d, 0x94, 0xc3, 0x51, 0xb4, 0x72, 0xc1}};

void* GetGameDevice() {
	// Two dereferences, and a null check between them. The renderer pointer
	// is a global that is null until the game builds one, and reading
	// +0x280 from a null renderer would fault - in a plugin, where the crash
	// is reported as Oblivion crashing.
	auto* renderer = *reinterpret_cast<UInt8**>(addr::kRendererPointer);
	if (renderer == nullptr) {
		return nullptr;
	}

	return *reinterpret_cast<void**>(renderer + addr::kRendererDeviceOffset);
}

DeviceKind IdentifyDevice(void* device) {
	if (device == nullptr) {
		return DeviceKind::Unavailable;
	}

	auto* unknown = static_cast<d3d11::Unknown*>(device);
	if (unknown->vtbl == nullptr || unknown->vtbl->QueryInterface == nullptr) {
		return DeviceKind::Unavailable;
	}

	// Every COM object begins with the same three methods, so IUnknown's
	// table is enough to ask this question without replicating any of
	// IDirect3DDevice9.
	void* interop = nullptr;
	const d3d11::ResultCode result =
		unknown->vtbl->QueryInterface(unknown, &kIID_D3D9VkInteropDevice, &interop);

	if (d3d11::Failed(result) || interop == nullptr) {
		return DeviceKind::Native;
	}

	// Released at once. OBVR has nothing to do with this interface yet, and a
	// reference held for a question already answered is only a reference to
	// leak - one that would keep the device alive past shutdown and turn into
	// a crash on exit, which nobody attributes correctly.
	d3d11::Release(interop);
	return DeviceKind::Dxvk;
}

const char* DeviceKindName(DeviceKind kind) {
	switch (kind) {
		case DeviceKind::Unavailable: return "unavailable";
		case DeviceKind::Native: return "native Direct3D 9";
		case DeviceKind::Dxvk: return "DXVK";
	}
	return "?";
}

}  // namespace obvr::render
