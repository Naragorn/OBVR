#pragma once

#include "core/Types.h"

// The parts of Vulkan, of DXVK's D3D9 interop and of OpenVR's Vulkan submit
// path that OBVR needs, replicated on the same terms as everything else here:
// read out of the headers rather than recalled, and only the pieces that are
// used.
//
// Sources:
//   * DXVK's src/d3d9/d3d9_interfaces.h for the interop interfaces
//   * KhronosGroup/Vulkan-Headers, include/vulkan/vulkan_core.h, for
//     VkImageCreateInfo and VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO
//   * ValveSoftware/openvr, headers/openvr.h, for VRVulkanTextureData_t
//
// Why this file exists at all: OpenVR's Submit takes no Direct3D 9 texture
// and never has, so Oblivion's picture cannot go straight to the compositor.
// DXVK renders D3D9 in Vulkan and publishes the Vulkan objects behind a
// texture, which turns an impossible conversion into a question of asking
// politely. Everything below is that question, written out.

namespace obvr::render::dxvk {

// Vulkan's handle types, and the one place a 32-bit process has to be careful.
//
// Dispatchable handles - instance, physical device, device, queue - are
// pointers, so four bytes here. Non-dispatchable ones, VkImage among them,
// are uint64_t on every platform, because Vulkan defines them that way rather
// than as pointers. Mixing the two sizes is what makes the structures below
// worth asserting rather than trusting.
using VkHandle = void*;
using VkImageHandle = unsigned long long;

// VkStructureType. Only the one value OBVR sets, and it has to be set:
// GetVulkanImageInfo requires sType to already say what the structure is.
constexpr UInt32 kStructureTypeImageCreateInfo = 14;

// VkImageLayout (vulkan_core.h). Two values, and the gap between them is a
// thing OBVR has to close rather than a detail.
//
// SteamVR's Vulkan documentation is explicit: an image passed to the runtime
// "should be in the VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL layout when passed
// to the SteamVR runtime, and will still be in that state when the submission
// work has finished executing". Oblivion's back buffer reports GENERAL. That
// is what ID3D9VkInteropDevice::TransitionTextureLayout is for, and knowing
// why that method exists is worth more than the constant.
constexpr UInt32 kImageLayoutGeneral = 1;
constexpr UInt32 kImageLayoutTransferSrcOptimal = 6;

// VkImageUsageFlagBits (vulkan_core.h). The two SteamVR requires of a
// submitted image: "must have been created with at least the following
// VkImageUsageFlags: VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
// VK_IMAGE_USAGE_SAMPLED_BIT".
//
// Unlike the layout, this one cannot be fixed after the fact. Usage is fixed
// when an image is created, so if Oblivion's back buffer lacks these there is
// no transition that helps - the frame would have to be copied into an image
// that has them.
constexpr UInt32 kImageUsageTransferSrc = 0x00000001;
constexpr UInt32 kImageUsageSampled = 0x00000004;

// VkFormat values SteamVR accepts, of which Oblivion's back buffer reports
// the third. The full list from the same page is R8G8B8A8_UNORM,
// R8G8B8A8_SRGB, B8G8R8A8_UNORM, B8G8R8A8_SRGB, R32G32B32A32_SFLOAT,
// R32G32B32_SFLOAT, R16G16B16A16_SFLOAT and A2R10G10B10_UINT_PACK32.
constexpr UInt32 kFormatB8G8R8A8Unorm = 44;

// VK_IMAGE_ASPECT_COLOR_BIT (vulkan_core.h)
constexpr UInt32 kImageAspectColor = 0x00000001;

// VkExtent3D (vulkan_core.h)
struct VkExtent3D {
	UInt32 width;
	UInt32 height;
	UInt32 depth;
};

// VkImageSubresourceRange (vulkan_core.h). Which part of an image a layout
// transition applies to - for a back buffer, all of the one and only part.
struct VkImageSubresourceRange {
	UInt32 aspectMask;
	UInt32 baseMipLevel;
	UInt32 levelCount;
	UInt32 baseArrayLayer;
	UInt32 layerCount;
};

// VkImageCreateInfo (vulkan_core.h), in full.
//
// In full rather than truncated even though OBVR reads three fields from it,
// because DXVK writes the whole thing: a short structure would be written
// past the end of. That is a stack corruption rather than a wrong answer.
struct VkImageCreateInfo {
	UInt32 sType;
	const void* pNext;
	UInt32 flags;
	UInt32 imageType;
	UInt32 format;
	VkExtent3D extent;
	UInt32 mipLevels;
	UInt32 arrayLayers;
	UInt32 samples;
	UInt32 tiling;
	UInt32 usage;
	UInt32 sharingMode;
	UInt32 queueFamilyIndexCount;
	const UInt32* pQueueFamilyIndices;
	UInt32 initialLayout;
};

// VRVulkanTextureData_t (openvr.h), what Submit takes when the texture type
// is Vulkan.
//
// The layout is the delicate part. m_nImage is eight bytes and everything
// after it is four, so the structure mixes alignments - and getting it wrong
// does not produce a diagnostic. It hands the compositor an image handle that
// is really half a device pointer.
struct VRVulkanTextureData {
	VkImageHandle image;
	VkHandle device;
	VkHandle physicalDevice;
	VkHandle instance;
	VkHandle queue;
	UInt32 queueFamilyIndex;
	UInt32 width;
	UInt32 height;
	UInt32 format;
	UInt32 sampleCount;
};

// The one assertion that needs no offsetof, and the one that matters most:
// an image handle is 64 bits even in a 32-bit process. If this ever became
// pointer-sized, every field after it would shift and the compositor would be
// handed half a pointer as an image.
//
// The per-field offsets are checked in dxvk_interop_test rather than here.
// offsetof comes from <cstddef>, which the freestanding Linux build does not
// have - the same reason the IVRSystem indices are pinned in a test rather
// than in OpenVRTypes.h.
static_assert(sizeof(VkImageHandle) == 8, "a Vulkan image handle is 64 bits everywhere");

// ID3D9VkInteropDevice, queried from Oblivion's IDirect3DDevice9.
//
// Method order from d3d9_interfaces.h, after IUnknown's three:
//
//    3 GetVulkanHandles         8 ReleaseSubmissionQueue
//    4 GetSubmissionQueue       9 LockDevice
//    5 TransitionTextureLayout 10 UnlockDevice
//    6 FlushRenderingCommands  11 WaitForResource
//    7 LockSubmissionQueue     12 CreateImage
//
// Only the ones OBVR calls are typed. The rest stay void* and hold their
// slots, which is what keeps the typed ones at the right index.
struct InteropDeviceVtbl {
	void* queryInterface;
	void* addRef;
	UInt32(__stdcall* Release)(void* self);

	void(__stdcall* GetVulkanHandles)(void* self, VkHandle* instance, VkHandle* physicalDevice,
	                                  VkHandle* device);

	// The queue index and the family index are different things, and both are
	// out-parameters. OpenVR wants the family one.
	void(__stdcall* GetSubmissionQueue)(void* self, VkHandle* queue, UInt32* queueIndex,
	                                    UInt32* queueFamilyIndex);

	// SteamVR wants a submitted image in TRANSFER_SRC_OPTIMAL; a back buffer
	// is in GENERAL. This is the method that closes that gap, and the reason
	// DXVK publishes it at all.
	//
	// The texture argument is the ID3D9VkInteropTexture, not the surface it
	// was queried from.
	void(__stdcall* TransitionTextureLayout)(void* self, void* texture,
	                                         const VkImageSubresourceRange* subresources,
	                                         UInt32 oldLayout, UInt32 newLayout);

	// Outstanding work has to reach the queue before the compositor reads the
	// image, or it reads a frame that has not finished being drawn.
	void(__stdcall* FlushRenderingCommands)(void* self);

	// The compositor submits on DXVK's own queue, and DXVK is using it. These
	// bracket that.
	void(__stdcall* LockSubmissionQueue)(void* self);
	void(__stdcall* ReleaseSubmissionQueue)(void* self);

	void* lockDevice;
	void* unlockDevice;
	void* waitForResource;
	void* createImage;
};

struct InteropDevice {
	const InteropDeviceVtbl* vtbl;
};

// ID3D9VkInteropTexture, queried from an IDirect3DSurface9 or texture. It has
// exactly one method of its own.
//
// pInfo must arrive with sType already set to VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO
// and queueFamilyIndexCount set to the length of pQueueFamilyIndices - which
// may be zero, and in practice is, since DXVK documents the sharing mode as
// always exclusive.
struct InteropTextureVtbl {
	void* queryInterface;
	void* addRef;
	UInt32(__stdcall* Release)(void* self);

	SInt32(__stdcall* GetVulkanImageInfo)(void* self, VkImageHandle* handle, UInt32* layout,
	                                      VkImageCreateInfo* info);
};

struct InteropTexture {
	const InteropTextureVtbl* vtbl;
};

// IDirect3DDevice9::GetBackBuffer sits at vtable index 18, right after
// Present at 17. Counted from Wine's include/d3d9.h, and an interface fact
// rather than a game one - it holds for every D3D9 implementation, DXVK
// included.
constexpr UInt32 kD3D9GetBackBufferIndex = 18;

// D3DBACKBUFFER_TYPE_MONO
constexpr UInt32 kBackBufferTypeMono = 0;

}  // namespace obvr::render::dxvk
