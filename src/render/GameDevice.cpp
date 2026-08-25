#include "render/GameDevice.h"

#include "game/GameAddresses.h"
#include "render/D3D11Types.h"
#include "render/DxvkInterop.h"

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

bool GetVulkanContext(void* device, VulkanContext& out) {
	if (device == nullptr) {
		return false;
	}

	auto* unknown = static_cast<d3d11::Unknown*>(device);
	if (unknown->vtbl == nullptr || unknown->vtbl->QueryInterface == nullptr) {
		return false;
	}

	void* raw = nullptr;
	if (d3d11::Failed(
			unknown->vtbl->QueryInterface(unknown, &kIID_D3D9VkInteropDevice, &raw)) ||
	    raw == nullptr) {
		return false;
	}

	auto* interop = static_cast<dxvk::InteropDevice*>(raw);
	if (interop->vtbl == nullptr || interop->vtbl->GetVulkanHandles == nullptr ||
	    interop->vtbl->GetSubmissionQueue == nullptr) {
		d3d11::Release(raw);
		return false;
	}

	VulkanContext filled;
	interop->vtbl->GetVulkanHandles(interop, &filled.instance, &filled.physicalDevice,
	                                &filled.device);
	interop->vtbl->GetSubmissionQueue(interop, &filled.queue, &filled.queueIndex,
	                                  &filled.queueFamilyIndex);

	d3d11::Release(raw);

	// All or nothing. A context with one null handle would be submitted and
	// fail inside the compositor, which reports it as a bad texture rather
	// than as a missing device - and the search would start in the wrong
	// place entirely.
	if (filled.instance == nullptr || filled.physicalDevice == nullptr ||
	    filled.device == nullptr || filled.queue == nullptr) {
		return false;
	}

	out = filled;
	return true;
}

bool GetBackBufferImage(void* device, BackBufferImage& out) {
	if (device == nullptr) {
		return false;
	}

	// GetBackBuffer is reached through the raw vtable rather than a declared
	// interface. Replicating IDirect3DDevice9 down to index 18 would mean
	// eighteen signatures to get right for the sake of calling one of them,
	// and every one of the other seventeen would be a chance to be wrong
	// about something OBVR never calls.
	using GetBackBufferFn = d3d11::ResultCode(__stdcall*)(void* self, UInt32 swapChain,
	                                                      UInt32 backBuffer, UInt32 type,
	                                                      void** surface);

	auto** vtbl = *reinterpret_cast<void***>(device);
	if (vtbl == nullptr) {
		return false;
	}

	auto getBackBuffer =
		reinterpret_cast<GetBackBufferFn>(vtbl[dxvk::kD3D9GetBackBufferIndex]);
	if (getBackBuffer == nullptr) {
		return false;
	}

	void* surface = nullptr;
	if (d3d11::Failed(getBackBuffer(device, 0, 0, dxvk::kBackBufferTypeMono, &surface)) ||
	    surface == nullptr) {
		return false;
	}

	// From here every path has to release the surface. GetBackBuffer adds a
	// reference, and this runs once per frame in the end - a leak here would
	// be a slow one, which is the kind that gets blamed on the game.
	auto* unknown = static_cast<d3d11::Unknown*>(surface);
	void* raw = nullptr;
	if (unknown->vtbl == nullptr || unknown->vtbl->QueryInterface == nullptr ||
	    d3d11::Failed(
			unknown->vtbl->QueryInterface(unknown, &kIID_D3D9VkInteropTexture, &raw)) ||
	    raw == nullptr) {
		d3d11::Release(surface);
		return false;
	}

	auto* texture = static_cast<dxvk::InteropTexture*>(raw);
	if (texture->vtbl == nullptr || texture->vtbl->GetVulkanImageInfo == nullptr) {
		d3d11::Release(raw);
		d3d11::Release(surface);
		return false;
	}

	// The structure arrives filled in rather than empty. DXVK requires sType
	// to already say what it is, and requires queueFamilyIndexCount to match
	// the array behind pQueueFamilyIndices - zero and a real pointer, since
	// DXVK documents the sharing mode as always exclusive and writes no
	// indices. A real pointer with a count of zero rather than a null one:
	// it costs nothing and removes a way to be wrong.
	UInt32 families[8] = {0};
	dxvk::VkImageCreateInfo info{};
	info.sType = dxvk::kStructureTypeImageCreateInfo;
	info.queueFamilyIndexCount = 0;
	info.pQueueFamilyIndices = families;

	unsigned long long image = 0;
	UInt32 layout = 0;
	const d3d11::ResultCode result =
		texture->vtbl->GetVulkanImageInfo(texture, &image, &layout, &info);

	d3d11::Release(raw);
	d3d11::Release(surface);

	if (d3d11::Failed(result) || image == 0) {
		return false;
	}

	out.image = image;
	out.layout = layout;
	out.format = info.format;
	out.width = info.extent.width;
	out.height = info.extent.height;
	out.sampleCount = info.samples;
	out.usage = info.usage;
	return true;
}

bool IsSubmittableImage(const BackBufferImage& image) {
	// Three conditions, all from SteamVR's Vulkan page, and each fails
	// differently.
	//
	// Usage is the one that cannot be repaired: it is fixed when the image is
	// created, so a back buffer without these bits has to be copied rather
	// than transitioned.
	if ((image.usage & dxvk::kImageUsageTransferSrc) == 0 ||
	    (image.usage & dxvk::kImageUsageSampled) == 0) {
		return false;
	}

	// Multisampled images are a separate path in the runtime, and OBVR does
	// not walk it. A back buffer normally has one sample.
	if (image.sampleCount != 1) {
		return false;
	}

	// An image of no size is not a picture, whatever else is right about it.
	return image.image != 0 && image.width > 0 && image.height > 0;
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
