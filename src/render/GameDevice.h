#pragma once

#include "core/Types.h"

namespace obvr::render {

// Oblivion's own Direct3D 9 device, and what kind of thing it turns out to be.
//
// This is the first step of 0.1.0 and the point where OBVR stops being a
// camera mod. Everything so far has worked on the scene graph: read a matrix,
// write a matrix. Putting Oblivion's picture in a headset means getting at
// the pixels, and the pixels belong to the device.
//
// The awkward part is that OpenVR's Submit has no entry for a Direct3D 9
// texture and never has - see HANDOFF.md section 13. The way out is DXVK,
// which renders D3D9 in Vulkan and hands out the Vulkan objects behind a
// texture through published interop interfaces. So the first thing worth
// knowing about the device is not what it can do but what it *is*.

// A 128-bit interface identifier, laid out as COM does it: one 32-bit field,
// two 16-bit fields, then eight bytes in order.
//
// Replicated rather than taken from a header for the usual reason - the
// SDK-free build has no guiddef.h - and it is one of the few places where a
// transcription error is genuinely dangerous. A wrong IID does not crash.
// QueryInterface simply answers "no such interface", OBVR concludes DXVK is
// absent, and the project quietly takes a different and much harder route for
// no reason at all. Hence the test that reads these bytes back against the
// textual form.
struct Guid {
	UInt32 data1;
	UInt16 data2;
	UInt16 data3;
	UInt8 data4[8];
};

// ID3D9VkInteropDevice, from DXVK's src/d3d9/d3d9_interfaces.h:
//   MIDL_INTERFACE("2eaa4b89-0107-4bdb-87f7-0f541c493ce0")
//
// Present in stock upstream DXVK. l4d2vr ships a fork, which made this route
// look far more expensive than it is; no fork is needed to ask this question
// or to answer it.
extern const Guid kIID_D3D9VkInteropDevice;

// ID3D9VkInteropTexture, same header:
//   MIDL_INTERFACE("d56344f5-8d35-46fd-806d-94c351b472c1")
//
// Not used yet. It is the one that yields the VkImage behind a D3D9 surface,
// which is what a submitted eye texture will eventually be.
extern const Guid kIID_D3D9VkInteropTexture;

// What is driving Direct3D 9 in this process.
enum class DeviceKind {
	// The renderer or the device could not be reached. Not necessarily a
	// fault: the renderer does not exist before the game has one.
	Unavailable,

	// A device that does not answer to DXVK's interop interface. Microsoft's
	// own Direct3D 9, in all likelihood - which is the case that needs the
	// D3D9Ex route instead.
	Native,

	// DXVK. The device answered to ID3D9VkInteropDevice, so the Vulkan
	// objects behind Oblivion's textures are reachable and the route this
	// project chose for 0.1.0 is open.
	Dxvk,
};

// Reads Oblivion's IDirect3DDevice9 out of the global renderer.
//
// Null until the game has built its renderer, which it has long before the
// camera hook first runs - so a null here from inside the hook means the
// address is wrong rather than that the timing is early.
void* GetGameDevice();

// Asks the device what it is. Costs one QueryInterface, and releases
// immediately: holding a reference to an interop interface OBVR is not using
// yet would only be a reference to leak.
DeviceKind IdentifyDevice(void* device);

const char* DeviceKindName(DeviceKind kind);

// The Vulkan objects behind Oblivion's Direct3D 9, when DXVK is what is
// serving it.
//
// Between them these are five of the ten fields OpenVR's VRVulkanTextureData_t
// wants; the other five come from the image itself. Which is the whole reason
// the DXVK route works at all: without these, a Vulkan image would be a handle
// with no context to interpret it in.
struct VulkanContext {
	void* instance = nullptr;
	void* physicalDevice = nullptr;
	void* device = nullptr;
	void* queue = nullptr;

	// Two different numbers with similar names. OpenVR wants the family
	// index; the queue index is which queue within that family, and is read
	// only because the call fills both and discarding one silently is how a
	// mix-up starts.
	UInt32 queueIndex = 0;
	UInt32 queueFamilyIndex = 0;
};

// Fills the context from the device. False if the device is not DXVK, or if
// any handle comes back null - a partially filled context is worse than none,
// because it would be submitted and fail somewhere unrelated.
bool GetVulkanContext(void* device, VulkanContext& out);

// The Vulkan image behind Oblivion's back buffer - the picture the game has
// just drawn, seen as Vulkan rather than as Direct3D 9.
//
// These are the other five fields of VRVulkanTextureData_t. Together with the
// context above, that is everything OpenVR needs to be handed Oblivion's own
// frame, which is what 0.1.0 is for.
struct BackBufferImage {
	// VkImage. Sixty-four bits even here, because Vulkan defines
	// non-dispatchable handles that way rather than as pointers.
	unsigned long long image = 0;

	// The layout the image will be in once outstanding commands have been
	// flushed - which is a promise about the future, not a description of
	// now, and the reason FlushRenderingCommands exists.
	UInt32 layout = 0;

	UInt32 format = 0;
	UInt32 width = 0;
	UInt32 height = 0;
	UInt32 sampleCount = 0;
};

// Reads it. False when the device is not DXVK, when the back buffer cannot be
// had, or when the surface does not carry a Vulkan image - which DXVK
// documents as possible for system-memory and scratch pools, though a back
// buffer is neither.
//
// Takes and releases its own references. Nothing is held onto: a back buffer
// is recreated whenever the device resets, so a stored one would outlive its
// image and be submitted after the image was gone.
bool GetBackBufferImage(void* device, BackBufferImage& out);

}  // namespace obvr::render
