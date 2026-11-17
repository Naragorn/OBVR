// Checks which library the backend reaches OpenVR through
// (vr/OpenVRLibrary.h): the shipped one, a stand-in named in [Debug]
// OpenVRLibrary, or nothing when the name is not a plain DLL's.

#include <cstdio>
#include <cstring>

#include "vr/OpenVRLibrary.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using namespace obvr::vr;

bool Is(const OpenVRLibraryPlan& plan, OpenVRLibrary kind, const char* name) {
	return plan.kind == kind && std::strcmp(plan.name, name) == 0;
}

void TestShipped() {
	std::printf("The shipped library\n");
	Check(Is(PlanOpenVRLibrary(""), OpenVRLibrary::Shipped, "openvr_api.dll"), "nothing configured: openvr_api.dll");
	Check(Is(PlanOpenVRLibrary(nullptr), OpenVRLibrary::Shipped, "openvr_api.dll"), "no text at all: openvr_api.dll");
	Check(Is(PlanOpenVRLibrary("openvr_api.dll"), OpenVRLibrary::Shipped, "openvr_api.dll"),
	      "its own name: still the shipped one, with its two places");
	Check(Is(PlanOpenVRLibrary("OpenVR_API.DLL"), OpenVRLibrary::Shipped, "openvr_api.dll"),
	      "its own name in other case: the same");
}

void TestStandIn() {
	std::printf("A stand-in\n");
	Check(Is(PlanOpenVRLibrary("openvr_mock.dll"), OpenVRLibrary::StandIn, "openvr_mock.dll"),
	      "openvr_mock.dll: the stand-in, by that name");
	Check(Is(PlanOpenVRLibrary("My-Runtime_2.DLL"), OpenVRLibrary::StandIn, "My-Runtime_2.DLL"),
	      "letters, digits, '-', '_' and an upper case .DLL: a stand-in");
}

void TestRefused() {
	std::printf("Names that are refused\n");
	const char* const refused[] = {
		"..\\openvr_mock.dll",      // a folder up
		"sub/openvr_mock.dll",      // a folder
		"sub\\openvr_mock.dll",     // a folder, Windows' way
		"C:openvr_mock.dll",        // a drive
		"openvr_mock..dll",         // two dots
		"openvr_mock",              // no .dll
		"openvr_mock.dll.txt",      // not ending in .dll
		".dll",                     // nothing before the ending
		"openvr mock.dll",          // a space
		"openvr_mock.dll ",         // a space after
	};
	for (const char* name : refused) {
		const OpenVRLibraryPlan plan = PlanOpenVRLibrary(name);
		Check(plan.kind == OpenVRLibrary::Refused && plan.name == name, name);
	}
	Check(!PlainDllName(nullptr), "no name is not a plain DLL's");
}

void TestNames() {
	std::printf("Names compared without case\n");
	Check(SameNameNoCase("a.DLL", "A.dll"), "the same but for case");
	Check(!SameNameNoCase("a.dll", "a.dl"), "one shorter");
	Check(!SameNameNoCase("a.dl", "a.dll"), "the other shorter");
	Check(!SameNameNoCase("a.dll", "b.dll"), "another letter");
	Check(SameNameNoCase("", ""), "two empty ones");
}

}  // namespace

int main() {
	TestShipped();
	TestStandIn();
	TestRefused();
	TestNames();
	std::printf("%s\n", g_failures == 0 ? "all checks passed" : "CHECKS FAILED");
	return g_failures == 0 ? 0 : 1;
}
