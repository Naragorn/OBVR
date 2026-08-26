// Checks the layouts at the seam between DXVK, Vulkan and OpenVR.
//
// Three separate projects meet in these structures, and OBVR replicates all
// of them rather than including their headers - so a mistake here is a
// mistake nobody else's compiler will catch. What makes it worth its own file
// is the failure mode: a shifted field does not produce an error. It hands
// the compositor a value that is genuinely there and genuinely wrong, and the
// symptom appears in the headset as a black screen or a garbled frame, three
// layers away from the cause.
//
// The particular trap is VRVulkanTextureData_t. Its first field is 64 bits
// even in a 32-bit process, because Vulkan defines non-dispatchable handles
// that way, while everything after it is a 32-bit pointer or integer. The
// structure therefore mixes alignments, and getting that wrong shifts every
// field after the first.
//
// Pure arithmetic, so this runs on Linux as well.

#include <cstddef>
#include <cstdio>

#include "render/DxvkInterop.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

void CheckOffset(std::size_t actual, std::size_t expected, const char* what) {
	if (actual != expected) {
		std::printf("        expected %zu, got %zu\n", expected, actual);
	}
	Check(actual == expected, what);
}

namespace dxvk = obvr::render::dxvk;

void TestVulkanTextureData() {
	std::printf("The structure OpenVR reads a Vulkan texture out of\n");

	using Data = dxvk::VRVulkanTextureData;

	// The whole reason this file exists. VkImage is uint64_t on every
	// platform, so the first field is eight bytes wide even where a pointer
	// is four - and the fields after it are placed accordingly.
	Check(sizeof(dxvk::VkImageHandle) == 8,
	      "an image handle is 64 bits, whatever the pointer size");
	CheckOffset(offsetof(Data, image), 0, "the image comes first");
	CheckOffset(offsetof(Data, device), 8, "and the device pointer follows all eight bytes");

	// Four handles in the order openvr.h declares them. A swap here would be
	// undetectable at compile time and fatal at run time: they are all
	// opaque pointers, so nothing complains about passing a physical device
	// where an instance belongs.
	const std::size_t handle = sizeof(dxvk::VkHandle);
	CheckOffset(offsetof(Data, physicalDevice), 8 + handle, "physical device second");
	CheckOffset(offsetof(Data, instance), 8 + handle * 2, "instance third");
	CheckOffset(offsetof(Data, queue), 8 + handle * 3, "queue fourth");

	// Then five 32-bit values, family index first.
	const std::size_t afterHandles = 8 + handle * 4;
	CheckOffset(offsetof(Data, queueFamilyIndex), afterHandles, "the family index follows them");
	CheckOffset(offsetof(Data, width), afterHandles + 4, "then width");
	CheckOffset(offsetof(Data, height), afterHandles + 8, "then height");
	CheckOffset(offsetof(Data, format), afterHandles + 12, "then format");
	CheckOffset(offsetof(Data, sampleCount), afterHandles + 16, "then the sample count");

	// The structure has to be at least as long as its last field, which
	// catches a declaration that ran out early. It is deliberately not
	// pinned to an exact size: the trailing padding is the compiler's
	// business and differs between 32 and 64 bit.
	Check(sizeof(Data) >= afterHandles + 20, "the structure holds all of its fields");
}

void TestImageCreateInfo() {
	std::printf("The structure DXVK writes image properties into\n");

	using Info = dxvk::VkImageCreateInfo;

	// Replicated in full rather than truncated, and that is the point worth
	// checking: DXVK writes the whole structure. A short one would be written
	// past the end of, which is stack corruption rather than a wrong value -
	// and stack corruption from a call that returned successfully is about
	// the least attributable fault there is.
	const std::size_t pointer = sizeof(void*);
	CheckOffset(offsetof(Info, sType), 0, "sType comes first, as every Vulkan structure does");
	CheckOffset(offsetof(Info, pNext), pointer, "pNext is aligned as a pointer");
	CheckOffset(offsetof(Info, format), pointer * 2 + 8, "format sits after flags and type");
	CheckOffset(offsetof(Info, extent), pointer * 2 + 12, "the extent follows the format");

	// Three uint32s, not a pointer to them.
	Check(sizeof(dxvk::VkExtent3D) == 12, "an extent is three 32-bit values");

	Check(sizeof(Info) >= offsetof(Info, initialLayout) + 4,
	      "the structure reaches its last field");

	// GetVulkanImageInfo refuses a structure whose sType does not already say
	// what it is, so the constant has to be the right one. 14 is
	// VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO in vulkan_core.h.
	Check(dxvk::kStructureTypeImageCreateInfo == 14,
	      "the image create info type is 14, as Vulkan numbers it");
}

void TestVtableIndices() {
	std::printf("Where the interop methods sit\n");

	// Function pointer indices, in units of one pointer so the check holds in
	// a 32-bit DLL and a 64-bit test binary alike. A wrong index calls a
	// different method with a different argument list, which on a stdcall
	// stack corrupts it rather than merely misbehaving.
	const std::size_t slot = sizeof(void*);

	CheckOffset(offsetof(dxvk::InteropDeviceVtbl, Release), slot * 2,
	            "Release is IUnknown's third entry");
	CheckOffset(offsetof(dxvk::InteropDeviceVtbl, GetVulkanHandles), slot * 3,
	            "GetVulkanHandles is the first method of its own");
	CheckOffset(offsetof(dxvk::InteropDeviceVtbl, GetSubmissionQueue), slot * 4,
	            "GetSubmissionQueue follows it");
	CheckOffset(offsetof(dxvk::InteropDeviceVtbl, FlushRenderingCommands), slot * 6,
	            "FlushRenderingCommands is at 6, past TransitionTextureLayout");
	CheckOffset(offsetof(dxvk::InteropDeviceVtbl, LockSubmissionQueue), slot * 7,
	            "and the queue lock pair at 7 and 8");
	CheckOffset(offsetof(dxvk::InteropDeviceVtbl, ReleaseSubmissionQueue), slot * 8,
	            "with release immediately after lock");

	// The texture interface has exactly one method of its own.
	CheckOffset(offsetof(dxvk::InteropTextureVtbl, GetVulkanImageInfo), slot * 3,
	            "GetVulkanImageInfo is the texture interface's only own method");

}

}  // namespace

int main() {
	std::printf("OBVR DXVK interop layout test\n\n");

	TestVulkanTextureData();
	std::printf("\n");
	TestImageCreateInfo();
	std::printf("\n");
	TestVtableIndices();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
