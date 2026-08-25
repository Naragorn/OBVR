// Checks the interface identifiers OBVR uses to ask Oblivion's Direct3D 9
// device what it is.
//
// Sixteen bytes each, transcribed by hand from DXVK's
// src/d3d9/d3d9_interfaces.h, and the reason they get a test of their own is
// how they fail. A wrong IID does not crash and does not misbehave.
// QueryInterface answers "no such interface", OBVR concludes DXVK is absent,
// and the project quietly commits to the harder D3D9Ex route for no reason -
// with nothing anywhere saying why. There is no worse shape of bug than one
// that produces a confident wrong answer.
//
// So the bytes are read back here against the textual form they came from,
// which is the one thing a transcription error cannot survive.
//
// Pure arithmetic, so this runs on Linux as well - and it is worth having
// there, because everything else about the device needs a running Oblivion.

#include <cstdio>

#include "render/DxvkInterop.h"
#include "render/GameDevice.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

// Renders a Guid back into the canonical 8-4-4-4-12 form, so the comparison
// is against the string as it appears in the header rather than against the
// same numbers written a second time - which would only prove the file agrees
// with itself.
void Format(const obvr::render::Guid& guid, char* out) {
	static const char* kDigits = "0123456789abcdef";
	int at = 0;

	auto byteOut = [&](UInt8 value) {
		out[at++] = kDigits[(value >> 4) & 0xF];
		out[at++] = kDigits[value & 0xF];
	};

	byteOut(static_cast<UInt8>(guid.data1 >> 24));
	byteOut(static_cast<UInt8>(guid.data1 >> 16));
	byteOut(static_cast<UInt8>(guid.data1 >> 8));
	byteOut(static_cast<UInt8>(guid.data1));
	out[at++] = '-';
	byteOut(static_cast<UInt8>(guid.data2 >> 8));
	byteOut(static_cast<UInt8>(guid.data2));
	out[at++] = '-';
	byteOut(static_cast<UInt8>(guid.data3 >> 8));
	byteOut(static_cast<UInt8>(guid.data3));
	out[at++] = '-';
	byteOut(guid.data4[0]);
	byteOut(guid.data4[1]);
	out[at++] = '-';
	for (int i = 2; i < 8; ++i) {
		byteOut(guid.data4[i]);
	}
	out[at] = '\0';
}

bool Same(const char* a, const char* b) {
	while (*a != '\0' && *b != '\0') {
		if (*a != *b) {
			return false;
		}
		++a;
		++b;
	}
	return *a == *b;
}

void CheckGuid(const obvr::render::Guid& guid, const char* expected, const char* what) {
	char rendered[40];
	Format(guid, rendered);
	if (!Same(rendered, expected)) {
		std::printf("        expected %s\n        got      %s\n", expected, rendered);
	}
	Check(Same(rendered, expected), what);
}

void TestInteropIds() {
	std::printf("The interop identifiers, against the header they came from\n");

	// MIDL_INTERFACE("2eaa4b89-0107-4bdb-87f7-0f541c493ce0")
	// ID3D9VkInteropDevice : public IUnknown
	CheckGuid(obvr::render::kIID_D3D9VkInteropDevice, "2eaa4b89-0107-4bdb-87f7-0f541c493ce0",
	          "ID3D9VkInteropDevice matches DXVK's d3d9_interfaces.h");

	// MIDL_INTERFACE("d56344f5-8d35-46fd-806d-94c351b472c1")
	// ID3D9VkInteropTexture
	CheckGuid(obvr::render::kIID_D3D9VkInteropTexture, "d56344f5-8d35-46fd-806d-94c351b472c1",
	          "ID3D9VkInteropTexture matches it too");

	// The two must not be the same, which sounds absurd until a copy-paste
	// puts one where the other belongs - and that fails exactly as quietly as
	// a mistyped digit does.
	char first[40];
	char second[40];
	Format(obvr::render::kIID_D3D9VkInteropDevice, first);
	Format(obvr::render::kIID_D3D9VkInteropTexture, second);
	Check(!Same(first, second), "and the two are not the same identifier");
}

void TestGuidLayout() {
	std::printf("The layout COM expects\n");

	// Sixteen bytes, in COM's order: a 32-bit field, two 16-bit fields, then
	// eight bytes that are *not* byte-swapped. Getting the last part wrong is
	// the classic GUID mistake, and it would leave the first half looking
	// right in a debugger while the interface was never found.
	Check(sizeof(obvr::render::Guid) == 16, "a Guid is sixteen bytes");
	Check(sizeof(obvr::render::Guid) ==
	          sizeof(UInt32) + sizeof(UInt16) + sizeof(UInt16) + 8,
	      "with no padding between its fields");

	const auto* bytes = reinterpret_cast<const UInt8*>(&obvr::render::kIID_D3D9VkInteropDevice);

	// The tail is stored in order, so the last byte of the identifier is the
	// last byte in memory. If it were swapped like the leading fields, this
	// would be 0x1c.
	Check(bytes[15] == 0xe0, "the trailing eight bytes are in order, not swapped");
	Check(obvr::render::kIID_D3D9VkInteropDevice.data4[0] == 0x87,
	      "and the eighth byte of the text is the first of the tail");
}

void TestNaming() {
	std::printf("What the log will say\n");

	// These strings go straight into OBVR.log and are the only thing that
	// will explain, months later, why 0.1.0 went the way it did.
	Check(obvr::render::DeviceKindName(obvr::render::DeviceKind::Dxvk) != nullptr,
	      "every kind has a name");
	Check(obvr::render::DeviceKindName(obvr::render::DeviceKind::Native) !=
	          obvr::render::DeviceKindName(obvr::render::DeviceKind::Dxvk),
	      "and native is not confusable with DXVK");
	Check(obvr::render::DeviceKindName(obvr::render::DeviceKind::Unavailable) !=
	          obvr::render::DeviceKindName(obvr::render::DeviceKind::Native),
	      "nor is a missing device confusable with a native one");

	// A device that could not be reached and one that is not DXVK call for
	// entirely different responses - one is "try again" and the other is
	// "take the other route" - so they must never collapse into one answer.
	Check(obvr::render::IdentifyDevice(nullptr) == obvr::render::DeviceKind::Unavailable,
	      "a null device is unavailable rather than native");
}

void TestSubmittability() {
	std::printf("Whether a frame could go to the compositor as it stands\n");

	// The three conditions come from SteamVR's Vulkan documentation, and the
	// baseline is what Oblivion's back buffer actually reported through DXVK:
	// 2560x1440, format 44 (B8G8R8A8_UNORM, which is on SteamVR's accepted
	// list), one sample, layout GENERAL.
	obvr::render::BackBufferImage good;
	good.image = 0x34A33C78;
	good.width = 2560;
	good.height = 1440;
	good.format = 44;
	good.sampleCount = 1;
	good.usage = obvr::render::dxvk::kImageUsageTransferSrc |
	             obvr::render::dxvk::kImageUsageSampled | 0x10;
	Check(obvr::render::IsSubmittableImage(good), "a real back buffer description passes");

	// Usage is the condition that cannot be repaired. It is fixed when the
	// image is created, so missing bits mean the frame has to be copied into
	// an image that has them - a different and much larger piece of work than
	// a layout transition, which is why it is worth knowing separately.
	obvr::render::BackBufferImage noTransfer = good;
	noTransfer.usage &= ~obvr::render::dxvk::kImageUsageTransferSrc;
	Check(!obvr::render::IsSubmittableImage(noTransfer),
	      "without TRANSFER_SRC it is refused rather than submitted and failed");

	obvr::render::BackBufferImage noSampled = good;
	noSampled.usage &= ~obvr::render::dxvk::kImageUsageSampled;
	Check(!obvr::render::IsSubmittableImage(noSampled), "and without SAMPLED as well");

	// Multisampled images take a different path in the runtime, one OBVR does
	// not walk.
	obvr::render::BackBufferImage multisampled = good;
	multisampled.sampleCount = 4;
	Check(!obvr::render::IsSubmittableImage(multisampled), "a multisampled image is refused");

	// A layout of GENERAL is deliberately *not* a reason to refuse. OpenVR
	// wants TRANSFER_SRC_OPTIMAL, but that is a transition away rather than a
	// property of the image - conflating the two would mean copying a frame
	// that only needed a barrier.
	obvr::render::BackBufferImage general = good;
	general.layout = obvr::render::dxvk::kImageLayoutGeneral;
	Check(obvr::render::IsSubmittableImage(general),
	      "the wrong layout is not a reason to refuse, only to transition");

	obvr::render::BackBufferImage empty;
	Check(!obvr::render::IsSubmittableImage(empty), "a default-constructed image is refused");
}

}  // namespace

int main() {
	std::printf("OBVR game device test\n\n");

	TestInteropIds();
	std::printf("\n");
	TestGuidLayout();
	std::printf("\n");
	TestNaming();
	std::printf("\n");
	TestSubmittability();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
